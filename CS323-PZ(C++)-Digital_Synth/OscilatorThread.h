#pragma once

#pragma comment(lib, "winmm.lib")

#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
using namespace std;

#include <Windows.h>

#ifndef I_FREQ_TYPE
#define I_FREQ_TYPE double
#endif

const double PI = 2.0 * acos(0.0);

template<class T>
class NoiseGenerator {

public:
	NoiseGenerator(wstring outputDevice, unsigned int sampleRate = 44100, unsigned int channels = 1, unsigned int blocks = 8, unsigned int blockSamples = 512) {
		Generate(outputDevice, sampleRate, channels, blocks, blockSamples);
	}

	~NoiseGenerator() {
		Destroy();
	}

	bool Generate(wstring outputDevice, unsigned int sampleRate = 44100, unsigned int channels = 1, unsigned int blocks = 8, unsigned int blockSamples = 512) {
		m_ready = false;
		m_sampleRate = sampleRate;
		m_channels = channels;
		m_blockCount = blocks;
		m_blockSamples = blockSamples;
		m_atomicFreeBlock= m_blockCount;
		m_blockCurrent = 0;
		m_blockMemoryPointer = nullptr;
		m_waveHeadersPointer = nullptr;

		m_userFunction = nullptr;

		// Validate device
		vector<wstring> devices = EnumerateDevices();
		auto d = std::find(devices.begin(), devices.end(), outputDevice);
		if (d != devices.end()) {
			// Device is available
			int deviceId = distance(devices.begin(), d);
			WAVEFORMATEX waveFormat;
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nSamplesPerSec = m_sampleRate;
			waveFormat.wBitsPerSample = sizeof(T) * 8;
			waveFormat.nChannels = m_channels;
			waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
			waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
			waveFormat.cbSize = 0;

			// Open Device if valid
			if (waveOutOpen(&m_hwDevice, deviceId, &waveFormat, (DWORD_PTR)waveOutProcWrap, (DWORD_PTR)this, CALLBACK_FUNCTION) != S_OK)
				return Destroy();
		}

		// Allocate Wave|Block Memory
		m_blockMemoryPointer = new T[m_blockCount * m_blockSamples];
		if (m_blockMemoryPointer == nullptr)
			return Destroy();
		ZeroMemory(m_blockMemoryPointer, sizeof(T) * m_blockCount * m_blockSamples);

		m_waveHeadersPointer = new WAVEHDR[m_blockCount];
		if (m_waveHeadersPointer == nullptr)
			return Destroy();
		ZeroMemory(m_waveHeadersPointer, sizeof(WAVEHDR) * m_blockCount);

		// Link headers to block memory
		for (unsigned int n = 0; n < m_blockCount; n++) {
			m_waveHeadersPointer[n].dwBufferLength = m_blockSamples * sizeof(T);
			m_waveHeadersPointer[n].lpData = (LPSTR)(m_blockMemoryPointer + (n * m_blockSamples));
		}

		m_ready = true;

		m_thread = thread(&NoiseGenerator::MainThread, this);

		// Start noise
		unique_lock<mutex> lm(m_muxBlockNotZero);
		m_blockNotZeroConditionVariable.notify_one();

		return true;
	}

	bool Destroy() {
		return false;
	}

	void Stop() {
		m_ready = false;
		m_thread.join();
	}

	virtual I_FREQ_TYPE UserProcess(int channel, I_FREQ_TYPE time) {
		return 0.0;
	}

	I_FREQ_TYPE GetTime() {
		return m_globalTime;
	}

public:
	static vector<wstring> EnumerateDevices() {
		int deviceCount = waveOutGetNumDevs();
		vector<wstring> deviceVector;
		WAVEOUTCAPS woc;
		for (int n = 0; n < deviceCount; n++)
			if (waveOutGetDevCaps(n, &woc, sizeof(WAVEOUTCAPS)) == S_OK)
				deviceVector.push_back(woc.szPname);
		return deviceVector;
	}

	void SetUserFunction(I_FREQ_TYPE(*func)(int, I_FREQ_TYPE)) {
		m_userFunction = func;
	}

	I_FREQ_TYPE clip(I_FREQ_TYPE sample, I_FREQ_TYPE max) {
		if (sample >= 0.0)
			return fmin(sample, max);
		else
			return fmax(sample, -max);
	}


private:
	I_FREQ_TYPE(*m_userFunction)(int, I_FREQ_TYPE);

	unsigned int m_sampleRate;
	unsigned int m_channels;
	unsigned int m_blockCount;
	unsigned int m_blockSamples;
	unsigned int m_blockCurrent;

	T* m_blockMemoryPointer;
	WAVEHDR* m_waveHeadersPointer;
	HWAVEOUT m_hwDevice;

	thread m_thread;
	atomic<bool> m_ready;
	atomic<unsigned int> m_atomicFreeBlock;
	condition_variable m_blockNotZeroConditionVariable;
	mutex m_muxBlockNotZero;

	atomic<I_FREQ_TYPE> m_globalTime;

	void waveOutProc(HWAVEOUT waveOut, UINT msg, DWORD param1, DWORD param2) {
		if (msg != WOM_DONE) return;

		m_atomicFreeBlock++;
		unique_lock<mutex> lm(m_muxBlockNotZero);
		m_blockNotZeroConditionVariable.notify_one();
	}

	static void CALLBACK waveOutProcWrap(HWAVEOUT waveOut, UINT msg, DWORD instance, DWORD param1, DWORD param2) {
		((NoiseGenerator*)instance)->waveOutProc(waveOut, msg, param1, param2);
	}

	void MainThread() {
		m_globalTime = 0.0;
		I_FREQ_TYPE timeStep = 1.0 / (I_FREQ_TYPE)m_sampleRate;

		T genericMaxSample = (T)pow(2, (sizeof(T) * 8) - 1) - 1;
		I_FREQ_TYPE maxSample = (I_FREQ_TYPE)genericMaxSample;
		T genericPreviousSample = 0;

		while (m_ready) {
			if (m_atomicFreeBlock== 0) {
				unique_lock<mutex> lm(m_muxBlockNotZero);
				while (m_atomicFreeBlock== 0) 
					m_blockNotZeroConditionVariable.wait(lm);
			}

			m_atomicFreeBlock--;

			// Prepare block for processing
			if (m_waveHeadersPointer[m_blockCurrent].dwFlags & WHDR_PREPARED)
				waveOutUnprepareHeader(m_hwDevice, &m_waveHeadersPointer[m_blockCurrent], sizeof(WAVEHDR));

			T genericNewSample = 0;
			int currentBlock = m_blockCurrent * m_blockSamples;

			for (unsigned int n = 0; n < m_blockSamples; n += m_channels) {
				for (unsigned int c = 0; c < m_channels; c++) {
					if (m_userFunction == nullptr)
						genericNewSample = (T)(clip(UserProcess(c, m_globalTime), 1.0) * maxSample);
					else
						genericNewSample = (T)(clip(m_userFunction(c, m_globalTime), 1.0) * maxSample);

					m_blockMemoryPointer[currentBlock + n + c] = genericNewSample;
					genericPreviousSample = genericNewSample;
				}

				m_globalTime = m_globalTime + timeStep;
			}

			// Send block to sound device
			waveOutPrepareHeader(m_hwDevice, &m_waveHeadersPointer[m_blockCurrent], sizeof(WAVEHDR));
			waveOutWrite(m_hwDevice, &m_waveHeadersPointer[m_blockCurrent], sizeof(WAVEHDR));
			m_blockCurrent++;
			m_blockCurrent %= m_blockCount;
		}
	}
};
