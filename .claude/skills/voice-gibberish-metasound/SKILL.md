---
name: voice-gibberish-metasound
description: Generates character voice lines for key actors and procedural gibberish speech for ambient NPCs and vendors, integrating them with timed MetaSounds in UE 5.8. Use when creating dialogue audio, vendor chatter, zombie vocalizations, or timed sound effects.
---

# Voice Generation & Procedural Gibberish MetaSound Skill

## Decision Tree
1. **Hero Moment / Key Lore / Major Vendor Greeting?**
   - Use **Full Generated Voice Stem** (rendered offline via local TTS/voice synth script with character formant shift and dungeon reverb impulse).
2. **Minor Moment / Browsing Vendor / Ambient Resting Ground Chatter?**
   - Use **Procedural Gibberish Synthesizer** (syllable-sliced phoneme bank driven at runtime by dialogue text string length and punctuation).

## Execution Protocol
1. Run `python3 .claude/skills/voice-gibberish-metasound/scripts/generate_phoneme_bank.py --voice-profile <profile_id>` to generate 24 micro-syllable `.wav` grains (`ba, ke, to, mu, ri...`) and key actor voice lines.
2. In UE 5.8, feed the syllable array into `MS_GibberishSynth` (MetaSound Source):
   - Parse each character of the subtitle string at `CharactersPerSecond`.
   - Map ASCII hash `CharCode % NumSyllables` to select a deterministic syllable grain so the same line always sounds identical.
   - Modulate pitch upward by `+3 semitones` on `?` and downward by `-2 semitones` on `.`.