/*
 * This file is part of EasyRPG Player.
 *
 * EasyRPG Player is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * EasyRPG Player is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with EasyRPG Player. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _AUDIO_SDL_H_
#define _AUDIO_SDL_H_

#include "audio.h"

#include <map>

#include <SDL.h>
#include <SDL_mixer.h>

#include "audio_decoder.h"

#define HAVE_MPG123
#ifdef HAVE_MPG123
#  include <mpg123.h>
#endif

struct SdlAudio : public AudioInterface {
	SdlAudio();
	~SdlAudio();

	void BGM_Play(std::string const&, int, int, int);
	void BGM_Pause();
	void BGM_Resume();
	void BGM_Stop();
	bool BGM_PlayedOnce();
	unsigned BGM_GetTicks();
	void BGM_Fade(int);
	void BGM_Volume(int);
	void BGM_Pitch(int);
	void BGS_Play(std::string const&, int, int, int);
	void BGS_Pause();
	void BGS_Resume();
	void BGS_Stop();
	void BGS_Fade(int);
	void ME_Play(std::string const&, int, int, int);
	void ME_Stop();
	void ME_Fade(int /* fade */);
	void SE_Play(std::string const&, int, int);
	void SE_Stop();
	void Update();

	void BGM_OnPlayedOnce();
	int BGS_GetChannel() const;

private:
	std::shared_ptr<Mix_Music> bgm;
	int bgm_volume;
	unsigned bgm_starttick = 0;
	bool bgm_stop = false;
	std::shared_ptr<Mix_Chunk> bgs;
	int bgs_channel;
	bool bgs_playing = false;
	bool bgs_stop = false;
	std::shared_ptr<Mix_Chunk> me;
	int me_channel;
	bool me_stopped_bgm;
	bool played_once = false;

	typedef std::map<int, std::shared_ptr<Mix_Chunk> > sounds_type;
	sounds_type sounds;

	std::unique_ptr<Mpg123Decoder> mpg123_decoder;
}; // class SdlAudio

#endif // _AUDIO_SDL_H_
