"""Офлайн-озвучка нейросетью Silero (русская модель v5).

Модель не читает латиницу и цифры, поэтому текст сначала нормализуется: английские
идентификаторы заменяются русской транскрипцией. «+» перед гласной — ударение в Silero.
"""
from __future__ import annotations

import hashlib
import os
import re
import subprocess
import wave

from core import SAMPLE_RATE

MODEL_ID = "v5_6_ru"
MODEL_URL = f"https://models.silero.ai/models/tts/ru/{MODEL_ID}.pt"
SPEAKERS = ["xenia", "baya", "kseniya", "aidar", "eugene"]
MODEL_RATE = 48000
CHUNK_CHARS = 400          # длинные реплики режем по предложениям
CHUNK_PAUSE = 0.12         # пауза между кусками одной реплики, с

# Сочетания, которые нельзя разбирать пословно
PHRASES = {
    "libstdc++": "либ эс тэ дэ си плюс плюс",
    "C++": "си плюс плюс",
    "C плюс плюс": "си плюс плюс",
    "C шарп": "си шарп",
    "i-й": "+итый",
    "splay-": "спл+эй-",
}

# Однобуквенные обозначения из формул: регистр важен
LETTERS = {
    "A": "а", "B": "бэ", "C": "цэ", "F": "эф", "L": "эл", "N": "эн", "R": "эр",
    "a": "э", "c": "цэ", "f": "эф", "h": "аш", "i": "и", "k": "ка", "m": "эм", "n": "эн",
    "x": "икс", "y": "+игрек", "z": "зэт",
}

WORDS = {
    "argument": "аргум+ент", "benchmark": "б+енчмарк", "button": "б+аттон",
    "chars": "чарз", "clock": "клок", "clone": "клоун", "contains": "конт+ейнс",
    "count": "к+аунт", "csv": "цэ эс вэ", "current": "к+аррент", "dear": "дир",
    "delete": "дил+ит", "detach": "дет+ач", "enumerator": "ин+ьюмерэйтор", "error": "+эррор",
    "false": "фолс", "find": "ф+айнд", "function": "ф+анкшн", "get": "гет", "git": "гит",
    "google": "гугл", "icollection": "ай кол+екшн", "ienumerable": "ай ин+ьюмерабл",
    "ienumerator": "ай ин+ьюмерэйтор", "imgui": "им гуи", "index": "+индекс",
    "insert": "инс+ерт", "invalid": "инв+алид", "invoke": "инв+оук", "length": "ленгс",
    "log": "лог", "logic": "л+оджик", "make": "мейк", "map": "мап", "min": "мин",
    "move": "мув", "next": "некст", "node": "н+оуд", "nodes": "н+оудз",
    "noexcept": "ноу эксепт", "not": "нот", "number": "н+амбер", "of": "оф",
    "option": "+опшн", "out": "+аут", "ptr": "п+ойнтер", "range": "рейндж", "read": "рид",
    "rebalance": "ребал+анс", "reduce": "редь+юс", "reset": "рис+ет", "result": "риз+алт",
    "set": "сет", "shared": "шэрд", "size": "сайз", "std": "эс тэ дэ", "steady": "ст+эди",
    "stl": "эс тэ эл", "strlen": "стр л+ен", "test": "тест", "thread": "тред", "to": "ту",
    "true": "тру", "try": "трай", "unique": "ю+ник", "update": "апд+ейт",
    "valuecodec": "в+элью к+одек", "vector": "в+ектор", "visit": "в+изит",
    "volatile": "в+олатайл", "where": "у+эр","windows": "в+индоус",
}


def normalize(text: str) -> str:
    for source, target in PHRASES.items():
        text = text.replace(source, target)

    def word(match: re.Match) -> str:
        token = match.group(0)
        if len(token) == 1:
            if token in LETTERS:
                return LETTERS[token]
        elif token.lower() in WORDS:
            return WORDS[token.lower()]
        raise ValueError(f"Silero: нет транскрипции для «{token}» в: {text}")

    text = re.sub(r"[A-Za-z]+", word, text)
    if re.search(r"\d", text):
        raise ValueError(f"Silero: цифры не поддерживаются: {text}")
    return text


def split_chunks(text: str) -> list[str]:
    sentences = re.split(r"(?<=[.!?…])\s+", text.strip())
    chunks: list[str] = []
    for sentence in sentences:
        if chunks and len(chunks[-1]) + 1 + len(sentence) <= CHUNK_CHARS:
            chunks[-1] += " " + sentence
        else:
            chunks.append(sentence)
    return chunks


def resample(pcm: bytes, rate_in: int, rate_out: int) -> bytes:
    """Пересэмплирование 16-битного моно через ffmpeg: монтаж видео ждёт SAMPLE_RATE."""
    return subprocess.run(
        ["ffmpeg", "-v", "error", "-f", "s16le", "-ar", str(rate_in), "-ac", "1", "-i", "-",
         "-f", "s16le", "-ar", str(rate_out), "-ac", "1", "-"],
        input=pcm, stdout=subprocess.PIPE, check=True).stdout


def load_model(cache_dir: str):
    import torch

    path = os.path.join(cache_dir, MODEL_ID + ".pt")
    if not os.path.isfile(path):
        os.makedirs(cache_dir, exist_ok=True)
        torch.hub.download_url_to_file(MODEL_URL, path)
    model = torch.package.PackageImporter(path).load_pickle("tts_models", "model")
    model.to(torch.device("cuda" if torch.cuda.is_available() else "cpu"))
    return model


def synthesize_silero(texts: list[str], cache_dir: str, speaker: str) -> list[str]:
    import torch

    os.makedirs(cache_dir, exist_ok=True)
    normalized = [normalize(text) for text in texts]  # ошибки транскрипции — до загрузки модели
    paths, missing = [], []
    for index, text in enumerate(normalized):
        key = f"{MODEL_ID}|{speaker}|{text}"
        digest = hashlib.sha1(key.encode("utf-8")).hexdigest()[:16]
        path = os.path.abspath(os.path.join(cache_dir, "silero_" + digest + ".wav"))
        paths.append(path)
        if not os.path.exists(path):
            missing.append(index)
    if not missing:
        return paths

    model = load_model(os.path.join(cache_dir, "..", "models"))
    silence = bytes(2 * int(MODEL_RATE * CHUNK_PAUSE))
    print(f"Silero {MODEL_ID}/{speaker}: {len(missing)} фраз")
    for number, index in enumerate(missing, 1):
        pcm = bytearray()
        for chunk in split_chunks(normalized[index]):
            with torch.no_grad():
                audio = model.apply_tts(text=chunk, speaker=speaker, sample_rate=MODEL_RATE)
            samples = (audio.clamp(-1, 1) * 32767).to(torch.int16).cpu().numpy().tobytes()
            if pcm:
                pcm += silence
            pcm += samples
        with wave.open(paths[index], "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(SAMPLE_RATE)
            w.writeframes(resample(bytes(pcm), MODEL_RATE, SAMPLE_RATE))
        print(f"  [{number}/{len(missing)}]", flush=True)
    return paths
