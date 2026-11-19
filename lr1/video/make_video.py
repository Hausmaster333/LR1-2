"""Сборка обучающего видео по ЛР-1.

    python make_video.py                  # полное видео в lr1_theory.mp4 (голос Irina)
    python make_video.py --preview 12     # PNG одного шага (для проверки вёрстки)

    Офлайн-озвучка Silero (нужен PyTorch, модель скачается при первом запуске):
    python make_video.py --tts silero --speaker xenia                    # lr1_theory_silero.mp4
    python make_video.py --tts silero --samples                          # пробы всех голосов

    Озвучка ElevenLabs (ключ берётся только из ELEVENLABS_API_KEY):
    python make_video.py --tts elevenlabs --list-voices
    python make_video.py --tts elevenlabs --voice <voice_id> --dry-run   # подсчёт символов
    python make_video.py --tts elevenlabs --voice <voice_id>             # lr1_theory_eleven.mp4
"""
import argparse
import os

from core import build_video, eleven_list_voices, render_step, synthesize, synthesize_eleven
from scenes import build

HERE = os.path.dirname(os.path.abspath(__file__))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preview", type=int, nargs="*", help="номера шагов для PNG-превью")
    parser.add_argument("--out", default=None)
    parser.add_argument("--tts", choices=["irina", "silero", "elevenlabs"], default="irina")
    parser.add_argument("--speaker", default="xenia", help="голос Silero")
    parser.add_argument("--samples", action="store_true", help="пробы всех голосов Silero")
    parser.add_argument("--voice", default=os.environ.get("ELEVENLABS_VOICE_ID"))
    parser.add_argument("--list-voices", action="store_true")
    parser.add_argument("--dry-run", action="store_true", help="только посчитать символы")
    args = parser.parse_args()

    steps = build().steps
    if args.preview is not None:
        preview_dir = os.path.join(HERE, "build", "preview")
        os.makedirs(preview_dir, exist_ok=True)
        for index, step in enumerate(steps):
            step.index, step.total = index, len(steps)
        numbers = args.preview or range(len(steps))
        for number in numbers:
            path = os.path.join(preview_dir, f"step_{number:03d}.png")
            render_step(steps[number], 1.0, 5).save(path)
        print(f"{len(steps)} шагов, превью в {preview_dir}")
        return

    tts = synthesize
    out = args.out or os.path.join(HERE, "lr1_theory.mp4")
    if args.tts == "silero":
        from tts_silero import SPEAKERS, synthesize_silero

        cache_dir = os.path.join(HERE, "build", "tts")
        if args.samples:
            spoken = [s.say for s in steps if s.say]
            # Вступление плюс реплики с формулами и английскими идентификаторами
            texts = [spoken[0]] + [t for t in spoken
                                   if "Правый поворот" in t or "IEnumerator" in t]
            for speaker in SPEAKERS:
                print(speaker, synthesize_silero(texts, cache_dir, speaker))
            return
        speaker = args.speaker

        def tts(texts: list[str], cache_dir: str) -> list[str]:
            return synthesize_silero(texts, cache_dir, speaker)

        out = args.out or os.path.join(HERE, "lr1_theory_silero.mp4")
    elif args.tts == "elevenlabs":
        api_key = os.environ.get("ELEVENLABS_API_KEY", "")
        if not api_key and not args.dry_run:
            raise SystemExit("Задайте переменную окружения ELEVENLABS_API_KEY")
        if args.list_voices:
            eleven_list_voices(api_key)
            return
        if not args.voice:
            raise SystemExit("Укажите --voice <voice_id> или ELEVENLABS_VOICE_ID "
                             "(список голосов: --list-voices)")
        voice = args.voice

        def tts(texts: list[str], cache_dir: str) -> list[str]:
            return synthesize_eleven(texts, cache_dir, api_key, voice, args.dry_run)

        out = args.out or os.path.join(HERE, "lr1_theory_eleven.mp4")

    build_video(steps, os.path.join(HERE, "build"), out, tts)


if __name__ == "__main__":
    main()
