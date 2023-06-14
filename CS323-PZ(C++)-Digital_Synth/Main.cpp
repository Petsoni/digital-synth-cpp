#include <list>
#include <iostream>
#include <algorithm>
using namespace std;
#include "OscilatorThread.h"
#include "Oscilator.h"

#define I_FREQ_TYPE double


namespace synthesizer {

	// Converts frequency (Hz) to angular velocity
	I_FREQ_TYPE ConvertToHz(const I_FREQ_TYPE hertz) {
		return hertz * 2.0 * PI;
	}

	struct BaseInstrument;

	struct Note {
		int id;
		I_FREQ_TYPE on;
		I_FREQ_TYPE off;
		bool active;
		BaseInstrument* channel;

		Note() {
			id = 0;
			on = 0.0;
			off = 0.0;
			active = false;
			channel = nullptr;
		}

	};

	const int SINE_WAVE = 0;
	const int SQUARE_WAVE = 1;
	const int TRIANGLE_WAVE = 2;
	const int SAW_WAVE = 3;
	const int NOISE = 4;

	I_FREQ_TYPE Oscillate(const I_FREQ_TYPE time, const I_FREQ_TYPE hertz, const int type = SINE_WAVE,
		const I_FREQ_TYPE lfoHertz = 0.0, const I_FREQ_TYPE lfoAmplitude = 0.0, I_FREQ_TYPE custom = 50.0) {

		I_FREQ_TYPE frequency = ConvertToHz(hertz) * time + lfoAmplitude * hertz * (sin(ConvertToHz(lfoHertz) * time));

		switch (type) {
		case SINE_WAVE:
			return sin(frequency);

		case SQUARE_WAVE:
			return sin(frequency) > 0 ? 1.0 : -1.0;

		case TRIANGLE_WAVE:
			return asin(sin(frequency)) * (2.0 / PI);

		case SAW_WAVE: {
			I_FREQ_TYPE output = 0.0;
			for (I_FREQ_TYPE n = 1.0; n < custom; n++)
				output += (sin(n * frequency)) / n;
			return output * (2.0 / PI);
		}

		case NOISE:
			return 2.0 * ((I_FREQ_TYPE)rand() / (I_FREQ_TYPE)RAND_MAX) - 1.0;

		default:
			return 0.0;
		}
	}

	const int DEFAULT_SCALE = 0;

	I_FREQ_TYPE Scale(const int noteId) {
		return 8 * pow(1.0594630943592952645618252949463, noteId);
	}

	/***************************************************************************************************************
	*********************************************** STRUCTS ********************************************************
	****************************************************************************************************************/
	struct Envelope {
		virtual I_FREQ_TYPE amplitude(const I_FREQ_TYPE time, const I_FREQ_TYPE timeOn, const I_FREQ_TYPE timeOff) = 0;
	};

	struct EnvelopeADSR : public Envelope {
		I_FREQ_TYPE attackTime;
		I_FREQ_TYPE decayTime;
		I_FREQ_TYPE sustainTime;
		I_FREQ_TYPE releaseTime;
		I_FREQ_TYPE startAmplitude;

		EnvelopeADSR() {
			attackTime = 0.1;
			decayTime = 0.1;
			sustainTime = 1.0;
			releaseTime = 0.2;
			startAmplitude = 1.0;
		}

		virtual I_FREQ_TYPE amplitude(const I_FREQ_TYPE time, const I_FREQ_TYPE timeOn, const I_FREQ_TYPE timeOff) {
			I_FREQ_TYPE amplitude = 0.0;
			I_FREQ_TYPE releaseAmplitude = 0.0;

			if (timeOn > timeOff) { // Note is on

				I_FREQ_TYPE lifeTime = time - timeOn;

				if (lifeTime <= attackTime)
					amplitude = (lifeTime / attackTime) * startAmplitude;

				if (lifeTime > attackTime && lifeTime <= (attackTime + decayTime))
					amplitude = ((lifeTime - attackTime) / decayTime) * (sustainTime - startAmplitude) + startAmplitude;

				if (lifeTime > (attackTime + decayTime))
					amplitude = sustainTime;
			}
			else { // Note is off

				I_FREQ_TYPE lifeTime = timeOff - timeOn;

				if (lifeTime <= attackTime)
					releaseAmplitude = (lifeTime / attackTime) * startAmplitude;

				if (lifeTime > attackTime && lifeTime <= (attackTime + decayTime))
					releaseAmplitude = ((lifeTime - attackTime) / decayTime) * (sustainTime - startAmplitude) + startAmplitude;

				if (lifeTime > (attackTime + decayTime))
					releaseAmplitude = sustainTime;

				amplitude = ((time - timeOff) / releaseTime) * (0.0 - releaseAmplitude) + releaseAmplitude;
			}

			// Amplitude should not be negative
			if (amplitude <= 0.01)
				amplitude = 0.0;

			return amplitude;
		}
	};

	I_FREQ_TYPE envelopeOutput(const I_FREQ_TYPE time, Envelope& envelopeOutput, const I_FREQ_TYPE timeOn, const I_FREQ_TYPE timeOff) {
		return envelopeOutput.amplitude(time, timeOn, timeOff);
	}

	struct BaseInstrument {
		I_FREQ_TYPE volume;
		synthesizer::EnvelopeADSR envelopeOutput;
		I_FREQ_TYPE maxLifeTme;
		wstring name;
		virtual I_FREQ_TYPE sound(const I_FREQ_TYPE time, synthesizer::Note n, bool& noteFinished) = 0;
	};

	struct Bell : public BaseInstrument {
		Bell() {
			envelopeOutput.attackTime = 0.01;
			envelopeOutput.decayTime = 1.0;
			envelopeOutput.sustainTime = 0.0;
			envelopeOutput.releaseTime = 1.0;
			maxLifeTme = 3.0;
			volume = 1.0;
			name = L"Bell";
		}

		virtual I_FREQ_TYPE sound(const I_FREQ_TYPE time, synthesizer::Note n, bool& noteFinished) {
			I_FREQ_TYPE amplitude = synthesizer::envelopeOutput(time, envelopeOutput, n.on, n.off);
			if (amplitude <= 0.0) noteFinished = true;

			I_FREQ_TYPE sound =
				1.00 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id + 12), synthesizer::SINE_WAVE, 5.0, 0.001)
				+ 0.50 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id + 24))
				+ 0.25 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id + 36));

			return amplitude * sound * volume;
		}

	};

	struct Bell8 : public BaseInstrument {
		Bell8() {
			envelopeOutput.attackTime = 0.01;
			envelopeOutput.decayTime = 0.5;
			envelopeOutput.sustainTime = 0.8;
			envelopeOutput.releaseTime = 1.0;
			maxLifeTme = 3.0;
			volume = 1.0;
			name = L"8-Bit Bell";
		}

		virtual I_FREQ_TYPE sound(const I_FREQ_TYPE time, synthesizer::Note n, bool& noteFinished) {
			I_FREQ_TYPE amplitude = synthesizer::envelopeOutput(time, envelopeOutput, n.on, n.off);
			if (amplitude <= 0.0) noteFinished = true;

			I_FREQ_TYPE sound =
				1.00 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id), synthesizer::SQUARE_WAVE, 5.0, 0.001)
				+ 0.50 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id + 12))
				+ 0.25 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id + 24));

			return amplitude * sound * volume;
		}

	};

	struct Harmonica : public BaseInstrument {
		Harmonica() {
			envelopeOutput.attackTime = 0.1;
			envelopeOutput.decayTime = 1.0;
			envelopeOutput.sustainTime = 0.95;
			envelopeOutput.releaseTime = 0.1;
			maxLifeTme = -1.0;
			name = L"Harmonica";
			volume = 0.3;
		}

		virtual I_FREQ_TYPE sound(const I_FREQ_TYPE time, synthesizer::Note n, bool& noteFinished) {
			I_FREQ_TYPE amplitude = synthesizer::envelopeOutput(time, envelopeOutput, n.on, n.off);
			if (amplitude <= 0.0) noteFinished = true;

			I_FREQ_TYPE sound =
				1.0 * synthesizer::Oscillate(n.on - time, synthesizer::Scale(n.id - 12), synthesizer::SAW_WAVE, 5.0, 0.001, 100)
				+ 1.00 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id), synthesizer::SQUARE_WAVE, 5.0, 0.001)
				+ 0.50 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id + 12), synthesizer::SQUARE_WAVE)
				+ 0.05 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id + 24), synthesizer::NOISE);

			return amplitude * sound * volume;
		}

	};

	struct Supersaw : public BaseInstrument {
		Supersaw() {
			envelopeOutput.attackTime = 0.05;
			envelopeOutput.decayTime = 1.0;
			envelopeOutput.sustainTime = 0.95;
			envelopeOutput.releaseTime = 0.1;
			maxLifeTme = -1.0;
			name = L"Supersaw";
			volume = 0.3;
		}

		virtual I_FREQ_TYPE sound(const I_FREQ_TYPE time, synthesizer::Note n, bool& noteFinished) {
			I_FREQ_TYPE amplitude = synthesizer::envelopeOutput(time, envelopeOutput, n.on, n.off);
			if (amplitude <= 0.0) noteFinished = true;

			I_FREQ_TYPE sound =
				1.0 * synthesizer::Oscillate(n.on - time, synthesizer::Scale(n.id - 12), synthesizer::SAW_WAVE, 5.0, 0.001, 100)
				+ 1.0 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id), synthesizer::SAW_WAVE, 5.0, 0.001)
				+ 0.50 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id + 12), synthesizer::SAW_WAVE)
				+ 0.05 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id + 24), synthesizer::NOISE);

			return amplitude * sound * volume;
		}

	};


	struct KickDrum : public BaseInstrument {
		KickDrum() {
			envelopeOutput.attackTime = 0.01;
			envelopeOutput.decayTime = 0.075;
			envelopeOutput.sustainTime = 0.0;
			envelopeOutput.releaseTime = 0.0;
			maxLifeTme = 1.5;
			name = L"Drum Kick";
			volume = 2.0;
		}

		virtual I_FREQ_TYPE sound(const I_FREQ_TYPE time, synthesizer::Note n, bool& noteFinished) {
			I_FREQ_TYPE amplitude = synthesizer::envelopeOutput(time, envelopeOutput, n.on, n.off);
			if (maxLifeTme > 0.0 && time - n.on >= maxLifeTme)	noteFinished = true;

			I_FREQ_TYPE sound =
				1.0 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id - 36), synthesizer::SINE_WAVE, 1.0, 1.0)
				+ 1.0 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id - 48), synthesizer::SINE_WAVE, 2, 2.0)
				+ 0.001 * synthesizer::Oscillate(time - n.on, 880.0, synthesizer::NOISE);

			return amplitude * sound * volume;
		}

	};

	struct SnareDrum : public BaseInstrument {
		SnareDrum() {
			envelopeOutput.attackTime = 0.0;
			envelopeOutput.decayTime = 0.125;
			envelopeOutput.sustainTime = 0.0;
			envelopeOutput.releaseTime = 0.0;
			maxLifeTme = 0.25;
			name = L"Drum Snare";
			volume = 1.0;
		}

		virtual I_FREQ_TYPE sound(const I_FREQ_TYPE time, synthesizer::Note n, bool& noteFinished) {
			I_FREQ_TYPE amplitude = synthesizer::envelopeOutput(time, envelopeOutput, n.on, n.off);
			if (maxLifeTme > 0.0 && time - n.on >= maxLifeTme)	noteFinished = true;

			I_FREQ_TYPE sound =
				0.5 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id), synthesizer::SINE_WAVE, 0.5, 1.0)
				+ 0.1 * synthesizer::Oscillate(time - n.on, 880.0, synthesizer::NOISE);

			return amplitude * sound * volume;
		}

	};


	struct HiHat : public BaseInstrument {
		HiHat() {
			envelopeOutput.attackTime = 0.01;
			envelopeOutput.decayTime = 0.025;
			envelopeOutput.sustainTime = 0.0;
			envelopeOutput.releaseTime = 0.0;
			maxLifeTme = 1.0;
			name = L"Drum HiHat";
			volume = 0.25;
		}

		virtual I_FREQ_TYPE sound(const I_FREQ_TYPE time, synthesizer::Note n, bool& noteFinished) {
			I_FREQ_TYPE amplitude = synthesizer::envelopeOutput(time, envelopeOutput, n.on, n.off);
			if (maxLifeTme > 0.0 && time - n.on >= maxLifeTme)	noteFinished = true;

			I_FREQ_TYPE sound =
				0.1 * synthesizer::Oscillate(time - n.on, synthesizer::Scale(n.id - 12), synthesizer::SQUARE_WAVE, 1.5, 1)
				+ 0.9 * synthesizer::Oscillate(time - n.on, 0, synthesizer::NOISE);

			return amplitude * sound * volume;
		}

	};


	struct DrumSequencer {

	public:
		int drumBeats;
		int drumSubBeats;
		I_FREQ_TYPE drumTemp;
		I_FREQ_TYPE drumBeatTime;
		I_FREQ_TYPE drumAccumulate;
		int drumCurrentBeat;
		int drumTotalBeats;

	public:

		struct Channel {
			BaseInstrument* instrument;
			wstring beat;
		};

	public:
		vector<Channel> vecChannel;
		vector<Note> vecNotes;

	public:

		DrumSequencer(float tempo = 120.0f, int beats = 4, int subbeats = 4) {
			drumBeats = beats;
			drumSubBeats = subbeats;
			drumTemp = tempo;
			drumBeatTime = (60.0f / drumTemp) / (float)drumSubBeats;
			drumCurrentBeat = 0;
			drumTotalBeats = drumSubBeats * drumBeats;
			drumAccumulate = 0;
		}

		int Update(I_FREQ_TYPE elapsedTime) {
			vecNotes.clear();

			drumAccumulate += elapsedTime;
			while (drumAccumulate >= drumBeatTime) {
				drumAccumulate -= drumBeatTime;
				drumCurrentBeat++;

				if (drumCurrentBeat >= drumTotalBeats)
					drumCurrentBeat = 0;

				int c = 0;
				for (auto v : vecChannel) {
					if (v.beat[drumCurrentBeat] == L'X' || v.beat[drumCurrentBeat] == L'x') {
						Note n;
						n.channel = vecChannel[c].instrument;
						n.active = true;
						n.id = 64;
						vecNotes.push_back(n);
					}
					c++;
				}
			}

			return vecNotes.size();
		}

		void AddInstrument(BaseInstrument* inst) {
			Channel c;
			c.instrument = inst;
			vecChannel.push_back(c);
		}
	};
}

vector<synthesizer::Note> vecNotes;
mutex muxNotes;

synthesizer::Bell bellInstrument;
synthesizer::Harmonica harmonicaInstrument;
synthesizer::Supersaw supersawInstrument;
synthesizer::KickDrum kickDrum;
synthesizer::SnareDrum snareDrum;
synthesizer::HiHat hiHat;

typedef bool(*lambda)(synthesizer::Note const& item);
template<class T>
void SafeRemove(T& v, lambda f) {

	auto n = v.begin();
	while (n != v.end())
		if (!f(*n))
			n = v.erase(n);
		else
			++n;
}

I_FREQ_TYPE GenerateNoise(int channel, I_FREQ_TYPE time) {
	unique_lock<mutex> lm(muxNotes);
	I_FREQ_TYPE mixedOutput = 0.0;

	for (auto& n : vecNotes) {
		bool noteFinished = false;
		I_FREQ_TYPE sound = 0;

		if (n.channel != nullptr)
			sound = n.channel->sound(time, n, noteFinished);

		mixedOutput += sound;

		if (noteFinished)
			n.active = false;
	}

	SafeRemove<vector<synthesizer::Note>>(vecNotes, [](synthesizer::Note const& item) { return item.active; });
	return mixedOutput * 0.2;
}

/***************************************************************************************************************
********************************************* MAIN START ******************************************************
****************************************************************************************************************/
int main() {

	vector<wstring> devices = NoiseGenerator<short>::EnumerateDevices();

	NoiseGenerator<short> sound(devices[0], 44100, 1, 8, 512);

	sound.SetUserFunction(GenerateNoise);

	wchar_t* screen = new wchar_t[80 * 30];
	HANDLE console = CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, NULL, CONSOLE_TEXTMODE_BUFFER, NULL);
	SetConsoleActiveScreenBuffer(console);
	DWORD bytesWritten = 0;

	auto draw = [&screen](int x, int y, wstring s) {
		for (size_t i = 0; i < s.size(); i++)
			screen[y * 120 + x + i] = s[i];
	};

	auto clockOldTime = chrono::high_resolution_clock::now();
	auto clockRealTime = chrono::high_resolution_clock::now();
	double elapsedTime = 0.0;
	double wallTime = 0.0;

	synthesizer::DrumSequencer seq(100.0);
	seq.AddInstrument(&kickDrum);
	seq.AddInstrument(&snareDrum);
	seq.AddInstrument(&hiHat);

	seq.vecChannel.at(0).beat = L"X...X...X...X..."; // Kick
	seq.vecChannel.at(1).beat = L"...X..X....X..X."; // Snare
	seq.vecChannel.at(2).beat = L"..X...X...X...X."; // HiHat

	while (1) {

		clockRealTime = chrono::high_resolution_clock::now();
		auto time_last_loop = clockRealTime - clockOldTime;
		clockOldTime = clockRealTime;
		elapsedTime = chrono::duration<I_FREQ_TYPE>(time_last_loop).count();
		wallTime += elapsedTime;
		I_FREQ_TYPE timeNow = sound.GetTime();

		int newNotes = seq.Update(elapsedTime);
		muxNotes.lock();
		for (int a = 0; a < newNotes; a++) {
			seq.vecNotes[a].on = timeNow;
			vecNotes.emplace_back(seq.vecNotes[a]);
		}
		muxNotes.unlock();

		for (int k = 0; k < 16; k++) {
			/***************************************************************************************************************
			************************************************ SOUND *********************************************************
			****************************************************************************************************************/

			short keyState = GetAsyncKeyState((unsigned char)("AWSEDFTGYHUJKOLP"[k]));

			muxNotes.lock();
			auto noteFound = find_if(vecNotes.begin(), vecNotes.end(), [&k](synthesizer::Note const& item) {
				return item.id == k + 64 && item.channel == &supersawInstrument; // Instrument played
				}
			);

			if (noteFound == vecNotes.end()) {

				if (keyState & 0x8000) {
					synthesizer::Note note;
					note.id = k + 64;
					note.on = timeNow;
					note.active = true;
					note.channel = &supersawInstrument;

					vecNotes.emplace_back(note);
				}
			}
			else {

				if (keyState & 0x8000) {

					if (noteFound->off > noteFound->on) {

						noteFound->on = timeNow;
						noteFound->active = true;
					}
				}
				else {

					if (noteFound->off < noteFound->on)
						noteFound->off = timeNow;
				}
			}
			muxNotes.unlock();
		}

		/***************************************************************************************************************
		* ******************************************** VISUALS *********************************************************
		****************************************************************************************************************/

		for (int i = 0; i < 80 * 30; i++) screen[i] = L' ';

		int n = 0;
		for (auto v : seq.vecChannel) {
			draw(2, 3 + n, v.instrument->name);
			draw(20, 3 + n, v.beat);
			n++;
		}

		draw(20 + seq.drumCurrentBeat, 1, L"|");

		vector<wstring> keyboardRows;

		keyboardRows.push_back(L",---,---,---,---,---,---,---,---,---,---,---,---,---,-------,");
		keyboardRows.push_back(L"|   |   |   |   |   |   |   |   |   |   |   |   |   |       |");
		keyboardRows.push_back(L"|---'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-----|");
		keyboardRows.push_back(L"|     |   | W | E |   | T | Y | U |   | O | P |   |   |     |");
		keyboardRows.push_back(L"|-----',--',--',--',--',--',--',--',--',--',--',--',--'-----|");
		keyboardRows.push_back(L"|      | A | S | D | F | G | H | J | K | L |   |   |        |");
		keyboardRows.push_back(L"|------'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-'-,-'--------|");
		keyboardRows.push_back(L"|        |   |   |   |   |   |   |   |   |   |   |          |");
		keyboardRows.push_back(L"|------,-',--'--,'---'---'---'---'---'---'-,-'---',--,------|");
		keyboardRows.push_back(L"|      |  |     |                          |      |  |      |");
		keyboardRows.push_back(L"'------'--'-----'--------------------------'------'--'------'");

		int drawYKeyboard = 7; // Start of y coordinate for drawing the keyboard

		for (int i = 0; i < keyboardRows.size(); i++) {
			draw(2, drawYKeyboard, keyboardRows[i]);
			drawYKeyboard++;
		}

		WriteConsoleOutputCharacter(console, screen, 80 * 30, { 0,0 }, &bytesWritten);

	}

	return 0;
}
