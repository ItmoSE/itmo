# Лабораторная работа №1 — Chroma key (вариант 5)

Реализованы два эквивалентных алгоритма: нативный обход пикселей на Python и библиотечная реализация на OpenCV. Цветовой интервал задаётся в RGB и включает обе границы.

## Запуск

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/python chroma_key.py foreground.png background.png result.png \
  --lower 0,160,0 --upper 120,255,120 --method opencv
```

Если размеры изображений различаются, фон масштабируется до размера первого изображения. Для нативной версии укажите `--method native`.

```bash
.venv/bin/pytest -q
.venv/bin/python benchmark.py
.venv/bin/python interval_experiments.py
```

Бенчмарк обрабатывает `pic1.jpg` и `pic2.jpg` на градиентном фоне
`assets/background.png`. Результаты сохраняются отдельно в `results/native` и
`results/opencv`, а измерения — в `results/benchmark.csv` и
`results/benchmark.svg`. Полный отчёт находится в `report.typ`.
Эксперимент с точным, узким, средним и широким цветовыми интервалами сохраняет
иллюстрации и статистику в `results/intervals`.
