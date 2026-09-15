"""Render the checked-in benchmark data; requires matplotlib (development only)."""
import json
from pathlib import Path
from statistics import mean
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "docs" / "benchmarks" / "2026-09-15.json"
data = json.loads(DATA.read_text(encoding="utf-8-sig"))
before, after = data["before"]["reports"], data["after"]["reports"]
assert len({row["dpi"] for row in before + after}) == 1, "DPI must match"
dpi = after[0]["dpi"]

def values(rows, mode, field):
    result = []
    for row in rows:
        if row["mode"] == mode:
            value = row
            for part in field.split("."):
                value = value[part]
            result.append(value)
    return result

plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 11,
                     "axes.spines.top": False, "axes.spines.right": False,
                     "axes.spines.left": False, "axes.edgecolor": "#dfe5eb",
                     "text.color": "#162b40", "axes.labelcolor": "#162b40",
                     "xtick.color": "#526376", "ytick.color": "#162b40"})
fig = plt.figure(figsize=(14, 11), facecolor="#f5f8fa")
fig.text(.065, .955, "CODEX BEER WIDGET  /  0.2.0", size=12, weight="bold", color="#147d76")
fig.text(.065, .910, "Меньше памяти. Спокойнее подсказка.", size=26, weight="bold")
fig.text(.065, .874, "Измерения до и после • Windows 11 • Ryzen 7 7735HS • 15 сентября 2026", size=11)

cards = [("static", "after.workingSetMiB", "Кружка без анимации", "MiB RAM"),
         ("hidden", "after.workingSetMiB", "Скрытый запуск", "MiB RAM"),
         ("static", "after.privateMiB", "Частная память: статика", "MiB")]
for x, (mode, field, label, unit) in zip([.065, .375, .685], cards):
    old, new = mean(values(before, mode, field)), mean(values(after, mode, field))
    fig.text(x, .818, label, size=12)
    fig.text(x, .778, f"{old:.1f} → {new:.1f}", size=25, weight="bold")
    fig.text(x, .747, f"{unit}   /   −{(1-new/old)*100:.0f}%", color="#147d76", size=12)

def panel(rect, modes, names, field, title, unit, old_modes=None):
    ax = fig.add_axes(rect, facecolor="#f5f8fa")
    y = np.arange(len(modes))
    for rows, offset, color, label in [(before, -.17, "#a5b2c0", "До"),
                                       (after, .17, "#188c83", "После")]:
        groups = [values(rows, (old_modes or modes)[i] if rows is before else mode, field)
                  for i, mode in enumerate(modes)]
        means = np.array([mean(g) for g in groups])
        error = np.array([[m-min(g) for g, m in zip(groups, means)],
                          [max(g)-m for g, m in zip(groups, means)]])
        ax.barh(y+offset, means, height=.28, color=color, label=label,
                xerr=error, error_kw={"ecolor": "#33485b", "capsize": 3})
        for pos, value, group in zip(y+offset, means, groups):
            ax.text(max(group)+.025*max(1, max(means)), pos, f"{value:.2f}", va="center", size=9)
    ax.set_yticks(y, names)
    ax.invert_yaxis()
    ax.set_title(title, loc="left", pad=18, weight="bold", size=14)
    ax.set_xlabel(unit, loc="right")
    ax.set_xlim(0, ax.get_xlim()[1]*1.22)
    ax.grid(axis="x", alpha=.14)
    ax.set_axisbelow(True)
    ax.tick_params(axis="y", length=0)

panel([.16, .425, .31, .245], ["hidden", "static", "normal", "clickthrough"],
      ["Скрытый", "Статика", "30 FPS", "Пропуск кликов"], "after.workingSetMiB",
      "Рабочая память", "MiB")
panel([.66, .425, .29, .245], ["normal", "smooth", "clickthrough", "hover800"],
      ["30 FPS", "60 FPS", "Пропуск кликов", "Подсказка"], "averageCpuOneCorePercent",
      "Загрузка CPU", "% одного логического ядра",
      ["normal", "smooth", "clickthrough", "hover-legacy"])
fig.legend(handles=[Patch(color="#a5b2c0", label="До"), Patch(color="#188c83", label="После")],
           loc="upper right", bbox_to_anchor=(.95, .729), ncol=2, frameon=False)
fig.text(.56, .37, "Цена экономии RAM: при постоянной анимации CPU выше.", size=10, color="#a1542c")

fig.text(.065, .338, "Подсказка появляется через 0,8 секунды", size=19, weight="bold")
fig.text(.065, .312, "Короткая остановка на 0,45 с больше не вызывает всплывание.\n"
         "Положение подсказки не пересчитывается на каждом движении мыши.", size=12, linespacing=1.6, va="top")
old_updates = mean(values(before, "hover-legacy", "tooltipUpdates"))
new_updates = mean(values(after, "hover800", "tooltipUpdates"))
fig.text(.70, .32, f"{old_updates:.0f} → {new_updates:.0f}", size=29, weight="bold", color="#147d76")
fig.text(.70, .287, "обновлений подсказки за 20 с", size=11)
fig.text(.065, .195, "Как читать результаты", size=12, weight="bold")
fig.text(.065, .163, "Средние двух прогонов по 20 с после 2 с прогрева; штрихи показывают минимум и максимум.\n"
         f"Высота 240 DIP, DPI {dpi}, остаток 70%. Синтетическая трасса курсора вызывает настоящую Win32-подсказку.\n"
         "Подсказка: настоящий курсор над кружкой; в остальных режимах — вне окна.\n"
         "Измерен только процесс виджета: Codex не включён. Рабочая сессия без изоляции фоновых приложений.\n"
         "Это короткие наблюдения на одном ПК, а не гарантии. Исходные числа: docs/benchmarks/2026-09-15.json.",
         size=10, linespacing=1.65, va="top", color="#526376")
for extension in ["png", "svg"]:
    fig.savefig(ROOT / "docs" / f"performance-020.{extension}", dpi=170, facecolor=fig.get_facecolor())
plt.close(fig)
