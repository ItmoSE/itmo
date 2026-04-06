import tkinter as tk
from tkinter import ttk, messagebox

import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401
import numpy as np


N_POINTS = 100_000
PLOT_POINTS_2D = 25_000
PLOT_POINTS_3D = 2_500
RNG = np.random.default_rng(42)


# ---------------------------------------------------------
# Вспомогательная линейная алгебра
# ---------------------------------------------------------
def normalize(v: np.ndarray) -> np.ndarray:
    n = np.linalg.norm(v)
    if n == 0:
        raise ValueError('Нельзя нормировать нулевой вектор')
    return v / n


def orthonormal_basis_from_normal(n: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    # Строим базис {u, v, n}, где u ⟂ n, v = n × u.
    n = normalize(n)
    helper = np.array([1.0, 0.0, 0.0]) if abs(n[0]) < 0.9 else np.array([0.0, 1.0, 0.0])
    u = normalize(np.cross(n, helper))
    v = normalize(np.cross(n, u))
    return u, v, n


# ---------------------------------------------------------
# Генераторы распределений
# ---------------------------------------------------------
def sample_triangle(v1: np.ndarray, v2: np.ndarray, v3: np.ndarray, n: int) -> np.ndarray:
    # Метод отражения:
    # P = V1 + r1(V2-V1) + r2(V3-V1), если r1+r2<=1,
    # иначе отражаем: r1=1-r1, r2=1-r2.
    r1 = RNG.random(n)
    r2 = RNG.random(n)
    mask = r1 + r2 > 1.0
    r1[mask] = 1.0 - r1[mask]
    r2[mask] = 1.0 - r2[mask]
    return v1 + r1[:, None] * (v2 - v1) + r2[:, None] * (v3 - v1)


def sample_disk(center: np.ndarray, normal: np.ndarray, radius: float, n: int) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    # Инверсия CDF по радиусу: r = R*sqrt(u), φ = 2πv.
    # Корень нужен из dA = r dr dφ.
    basis_u, basis_v, nrm = orthonormal_basis_from_normal(normal)
    u = RNG.random(n)
    phi = RNG.random(n) * 2.0 * np.pi
    r = radius * np.sqrt(u)
    pts = center + (r * np.cos(phi))[:, None] * basis_u + (r * np.sin(phi))[:, None] * basis_v
    return pts, basis_u, basis_v


def sample_sphere(n: int) -> np.ndarray:
    # Теорема Архимеда: равномерно по сфере, если z ~ U(-1,1), φ ~ U(0,2π).
    phi = RNG.random(n) * 2.0 * np.pi
    z = RNG.random(n) * 2.0 - 1.0
    r_xy = np.sqrt(1.0 - z**2)
    return np.column_stack((r_xy * np.cos(phi), r_xy * np.sin(phi), z))


def sample_cosine_hemisphere(normal: np.ndarray, n: int) -> np.ndarray:
    # Косинусный закон: p(ω) = cosθ/π, cosθ = ω·N.
    # Метод Малли: x = sqrt(r)cosφ, y = sqrt(r)sinφ, z = sqrt(1-r).
    basis_u, basis_v, nrm = orthonormal_basis_from_normal(normal)
    r1 = RNG.random(n)
    r2 = RNG.random(n)
    phi = 2.0 * np.pi * r1
    z = np.sqrt(r2)
    radial = np.sqrt(1.0 - r2)
    dirs = radial[:, None] * np.cos(phi)[:, None] * basis_u + radial[:, None] * np.sin(phi)[:, None] * basis_v + z[:, None] * nrm
    return dirs / np.linalg.norm(dirs, axis=1, keepdims=True)


# ---------------------------------------------------------
# Вспомогательные проверки и картинки доказательства
# ---------------------------------------------------------
def counts_in_neighborhoods(points: np.ndarray, centers: np.ndarray, eps: float) -> list[int]:
    # Окрестность: ||P - C_i|| < eps.
    return [int(np.sum(np.linalg.norm(points - c, axis=1) < eps)) for c in centers]


def triangle_inside(points: np.ndarray, v1: np.ndarray, v2: np.ndarray, v3: np.ndarray) -> np.ndarray:
    # Проверка через площади / знаки в 2D после проекции на плоскость XY.
    x, y = points[:, 0], points[:, 1]
    x1, y1 = v1[:2]
    x2, y2 = v2[:2]
    x3, y3 = v3[:2]
    det = (y2 - y3) * (x1 - x3) + (x3 - x2) * (y1 - y3)
    a = ((y2 - y3) * (x - x3) + (x3 - x2) * (y - y3)) / det
    b = ((y3 - y1) * (x - x3) + (x1 - x3) * (y - y3)) / det
    c = 1.0 - a - b
    return (a >= -1e-9) & (b >= -1e-9) & (c >= -1e-9)


# ---------------------------------------------------------
# Базовый класс вкладки GUI
# ---------------------------------------------------------
class ProofTab(ttk.Frame):
    def __init__(self, master, title: str):
        super().__init__(master)
        self.title = title

        top = ttk.Frame(self)
        top.pack(side=tk.TOP, fill=tk.X, padx=8, pady=8)
        ttk.Button(top, text='Пересчитать', command=self.refresh).pack(side=tk.LEFT)
        ttk.Label(top, text=title).pack(side=tk.LEFT, padx=12)

        self.info = tk.Text(self, height=12, wrap='word')
        self.info.pack(side=tk.TOP, fill=tk.X, padx=8, pady=(0, 8))

        self.figure = Figure(figsize=(11.5, 6.2), dpi=100)
        self.canvas = FigureCanvasTkAgg(self.figure, master=self)
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=8, pady=8)

    def set_info(self, text: str) -> None:
        self.info.delete('1.0', tk.END)
        self.info.insert(tk.END, text)

    def refresh(self) -> None:
        raise NotImplementedError


class TriangleTab(ProofTab):
    def __init__(self, master):
        super().__init__(master, 'Задача 1 — Треугольник: доказательство через окрестности')
        self.refresh()

    def refresh(self) -> None:
        v1 = np.array([0.0, 0.0, 0.0])
        v2 = np.array([10.0, 0.0, 0.0])
        v3 = np.array([5.0, 9.0, 0.0])
        pts = sample_triangle(v1, v2, v3, N_POINTS)
        pts2 = pts[:, :2]

        centers = np.array([
            [2.0, 1.5],
            [5.0, 1.5],
            [8.0, 1.5],
            [3.5, 4.5],
            [6.5, 4.5],
            [5.0, 7.2],
        ])
        eps = 0.8
        counts = counts_in_neighborhoods(pts2, centers, eps)
        inside = triangle_inside(pts, v1, v2, v3)

        self.figure.clear()
        ax1 = self.figure.add_subplot(121)
        ax2 = self.figure.add_subplot(122)

        draw = pts2[:PLOT_POINTS_2D]
        ax1.scatter(draw[:, 0], draw[:, 1], s=1, alpha=0.2)
        ax1.add_patch(plt.Polygon([v1[:2], v2[:2], v3[:2]], fill=False, edgecolor='black', lw=2))
        for c, cnt in zip(centers, counts):
            ax1.add_patch(plt.Circle(c, eps, facecolor='red', edgecolor='red', alpha=0.15, lw=2))
            ax1.text(c[0], c[1], str(cnt), ha='center', va='center', fontsize=11, fontweight='bold',
                     bbox=dict(fc='white', ec='none', alpha=0.7))
        ax1.set_title(f'Одинаковые окрестности радиуса ε = {eps}')
        ax1.set_aspect('equal')
        ax1.grid(True, alpha=0.3)

        bars = ax2.bar(range(1, len(counts) + 1), counts, edgecolor='black', width=0.6)
        avg = float(np.mean(counts))
        ax2.axhline(avg, color='red', ls='--', lw=2, label=f'среднее = {avg:.1f}')
        for bar, cnt in zip(bars, counts):
            ax2.text(bar.get_x() + bar.get_width() / 2, cnt + 2, str(cnt), ha='center', va='bottom', fontsize=10)
        ax2.set_xlabel('Номер окрестности')
        ax2.set_ylabel('Число точек')
        ax2.set_title('В одинаковых окрестностях чисел точек примерно поровну')
        ax2.legend()
        ax2.grid(True, alpha=0.3, axis='y')

        self.set_info(
            'Алгоритм генерации:\n'
            '1) Берём r1, r2 ~ U(0,1).\n'
            '2) Если r1 + r2 > 1, делаем отражение: r1 = 1-r1, r2 = 1-r2.\n'
            '3) Строим точку P = V1 + r1(V2-V1) + r2(V3-V1).\n\n'
            'Как работает доказательство сейчас:\n'
            'Мы берём несколько одинаковых окрестностей внутри треугольника и считаем, сколько случайных точек попало в каждую. '
            'Если распределение равномерное, то вероятность попасть в область зависит только от её площади. '
            'Значит, у одинаковых окрестностей площади одинаковые, и числа точек должны быть близки.\n\n'
            f'Проверка принадлежности: внутри треугольника оказалось {int(np.sum(inside))} из {N_POINTS} точек.'
        )
        self.canvas.draw()


class DiskTab(ProofTab):
    def __init__(self, master):
        super().__init__(master, 'Задача 2 — Круг: доказательство через окрестности')
        self.refresh()

    def refresh(self) -> None:
        radius = 5.0
        center = np.array([0.0, 0.0, 0.0])
        normal = np.array([0.0, 0.0, 1.0])
        pts, _, _ = sample_disk(center, normal, radius, N_POINTS)
        pts2 = pts[:, :2]

        centers = np.array([
            [0.0, 0.0],
            [2.2, 0.0],
            [-2.2, 0.0],
            [0.0, 2.2],
            [0.0, -2.2],
            [2.6, 2.4],
        ])
        eps = 0.75
        counts = counts_in_neighborhoods(pts2, centers, eps)
        radial = np.linalg.norm(pts2, axis=1)
        inside = radial <= radius + 1e-9

        self.figure.clear()
        ax1 = self.figure.add_subplot(121)
        ax2 = self.figure.add_subplot(122)

        draw = pts2[:PLOT_POINTS_2D]
        ax1.scatter(draw[:, 0], draw[:, 1], s=1, alpha=0.2)
        ax1.add_patch(plt.Circle((0.0, 0.0), radius, fill=False, edgecolor='black', lw=2))
        for c, cnt in zip(centers, counts):
            ax1.add_patch(plt.Circle(c, eps, facecolor='red', edgecolor='red', alpha=0.15, lw=2))
            ax1.text(c[0], c[1], str(cnt), ha='center', va='center', fontsize=11, fontweight='bold',
                     bbox=dict(fc='white', ec='none', alpha=0.7))
        ax1.set_xlim(-6.2, 6.2)
        ax1.set_ylim(-6.2, 6.2)
        ax1.set_aspect('equal')
        ax1.set_title(f'Одинаковые окрестности радиуса ε = {eps}')
        ax1.grid(True, alpha=0.3)

        bars = ax2.bar(range(1, len(counts) + 1), counts, edgecolor='black', width=0.6)
        avg = float(np.mean(counts))
        ax2.axhline(avg, color='red', ls='--', lw=2, label=f'среднее = {avg:.1f}')
        for bar, cnt in zip(bars, counts):
            ax2.text(bar.get_x() + bar.get_width() / 2, cnt + 2, str(cnt), ha='center', va='bottom', fontsize=10)
        ax2.set_xlabel('Номер окрестности')
        ax2.set_ylabel('Число точек')
        ax2.set_title('Одинаковые площади окрестностей дают близкие числа попаданий')
        ax2.legend()
        ax2.grid(True, alpha=0.3, axis='y')

        self.set_info(
            'Алгоритм генерации:\n'
            '1) Берём u ~ U(0,1), φ ~ U(0,2π).\n'
            '2) Радиус задаём по формуле r = R*sqrt(u).\n'
            '3) Точка диска: P = C + r(cosφ·e1 + sinφ·e2).\n\n'
            'Как работает доказательство сейчас:\n'
            'Мы снова сравниваем одинаковые окрестности. Принцип тот же: '
            'при равномерном распределении число точек в области пропорционально площади этой области. '
            'Поэтому одинаковые маленькие круги внутри диска должны содержать примерно одинаковое число точек, '
            'независимо от того, находятся ли они ближе к центру или ближе к краю.\n\n'
            f'Проверка принадлежности: внутри круга оказалось {int(np.sum(inside))} из {N_POINTS} точек.'
        )
        self.canvas.draw()


class SphereTab(ProofTab):
    def __init__(self, master):
        super().__init__(master, 'Задача 3 — Сфера: доказательство через телесные углы')
        self.refresh()

    def refresh(self) -> None:
        pts = sample_sphere(N_POINTS)
        alpha = np.radians(25.0)
        cos_alpha = np.cos(alpha)
        omega = 2.0 * np.pi * (1.0 - cos_alpha)
        expected = N_POINTS * omega / (4.0 * np.pi)

        cone_dirs = np.array([
            [0.0, 0.0, 1.0],
            [0.0, 0.0, -1.0],
            [1.0, 0.0, 0.0],
            [0.0, 1.0, 0.0],
            [-1.0, 0.0, 0.0],
            [1.0, 1.0, 1.0],
        ], dtype=float)
        cone_dirs = cone_dirs / np.linalg.norm(cone_dirs, axis=1, keepdims=True)
        counts = [int(np.sum(pts @ d > cos_alpha)) for d in cone_dirs]

        self.figure.clear()
        ax1 = self.figure.add_subplot(121, projection='3d')
        ax2 = self.figure.add_subplot(122)

        vis = pts[:PLOT_POINTS_3D]
        masks = [vis @ d > cos_alpha for d in cone_dirs]
        colors = ['tab:blue', 'tab:orange', 'tab:green', 'tab:red', 'tab:purple', 'tab:brown']
        outside = ~np.any(np.column_stack(masks), axis=1)
        ax1.scatter(vis[outside, 0], vis[outside, 1], vis[outside, 2], s=2, alpha=0.15, color='lightgray')

        t = np.linspace(0.0, 2.0 * np.pi, 120)
        for d, mask, color, cnt in zip(cone_dirs, masks, colors, counts):
            ax1.scatter(vis[mask, 0], vis[mask, 1], vis[mask, 2], s=4, alpha=0.8, color=color)
            if abs(d[2]) < 0.9:
                u = normalize(np.cross(d, np.array([0.0, 0.0, 1.0])))
            else:
                u = normalize(np.cross(d, np.array([1.0, 0.0, 0.0])))
            v = np.cross(d, u)
            circle = np.cos(alpha) * d[None, :] + np.sin(alpha) * np.cos(t)[:, None] * u[None, :] + np.sin(alpha) * np.sin(t)[:, None] * v[None, :]
            ax1.plot(circle[:, 0], circle[:, 1], circle[:, 2], color=color, lw=2)
            ax1.text(d[0] * 1.22, d[1] * 1.22, d[2] * 1.22, str(cnt), color=color, fontsize=10, fontweight='bold')

        ax1.set_title('Одинаковые конусы с одним и тем же телесным углом Ω')
        ax1.set_xlabel('X')
        ax1.set_ylabel('Y')
        ax1.set_zlabel('Z')

        labels = ['Север', 'Юг', 'X', 'Y', '-X', 'Диагональ']
        bars = ax2.bar(labels, counts, color=colors, edgecolor='black', width=0.65)
        ax2.axhline(expected, color='red', ls='--', lw=2, label=f'ожидание = {expected:.1f}')
        for bar, cnt in zip(bars, counts):
            ax2.text(bar.get_x() + bar.get_width() / 2, cnt + 2, str(cnt), ha='center', va='bottom', fontsize=10)
        ax2.set_ylabel('Число точек в конусе')
        ax2.set_title('Равные телесные углы дают близкие числа точек')
        ax2.grid(True, alpha=0.3, axis='y')
        ax2.legend()

        self.set_info(
            'Алгоритм генерации:\n'
            '1) Берём φ ~ U(0,2π).\n'
            '2) Берём z ~ U(-1,1).\n'
            '3) Вычисляем x = sqrt(1-z²)cosφ, y = sqrt(1-z²)sinφ.\n\n'
            'Как работает доказательство сейчас:\n'
            'Для сферы сравнивать плоские окрестности неудобно, поэтому используются конусы с одинаковым телесным углом Ω. '
            'Если распределение по поверхности сферы равномерное, то вероятность попасть в кусок поверхности зависит только от его площади, '
            'а на сфере площадь сферического колпака, вырезанного конусом, определяется именно телесным углом. '
            'Поэтому одинаковые конусы, направленные в разные стороны, должны захватывать примерно одинаковое число точек.\n\n'
            f'Здесь Ω = 2π(1-cosα) = {omega:.4f} ср, ожидаемое число точек в каждом конусе ≈ {expected:.1f}.'
        )
        self.canvas.draw()


class CosineTab(ProofTab):
    def __init__(self, master):
        super().__init__(master, 'Задача 4 — Косинусное распределение: сравнение с теорией')
        self.refresh()

    def refresh(self) -> None:
        normal = np.array([0.0, 0.0, 1.0])
        pts = sample_cosine_hemisphere(normal, N_POINTS)
        theta = np.arccos(np.clip(pts[:, 2], 0.0, 1.0))
        theta_deg = np.degrees(theta)

        band_edges_deg = np.linspace(0.0, 90.0, 7)
        band_colors = plt.get_cmap('RdYlGn')(np.linspace(0.9, 0.1, 6))

        n_bins = 12
        theta_edges = np.linspace(0.0, np.pi / 2.0, n_bins + 1)
        theta_centers = 0.5 * (theta_edges[:-1] + theta_edges[1:])
        counts, _ = np.histogram(theta, bins=theta_edges)
        delta_omega = 2.0 * np.pi * (np.cos(theta_edges[:-1]) - np.cos(theta_edges[1:]))
        density_hat = counts / (N_POINTS * delta_omega)
        density_theory = np.cos(theta_centers) / np.pi

        self.figure.clear()
        ax1 = self.figure.add_subplot(121)
        ax2 = self.figure.add_subplot(122)

        r_flat = np.sqrt(pts[:PLOT_POINTS_2D, 0] ** 2 + pts[:PLOT_POINTS_2D, 1] ** 2)
        z_flat = pts[:PLOT_POINTS_2D, 2]
        theta_vis = theta_deg[:PLOT_POINTS_2D]
        for i in range(6):
            mask = (theta_vis >= band_edges_deg[i]) & (theta_vis < band_edges_deg[i + 1])
            ax1.scatter(r_flat[mask], z_flat[mask], s=1, alpha=0.35, color=band_colors[i])
        for deg in band_edges_deg:
            ang = np.radians(deg)
            ax1.plot([0.0, np.sin(ang)], [0.0, np.cos(ang)], 'k--', lw=1, alpha=0.6)
        arc = np.linspace(0.0, np.pi / 2.0, 150)
        ax1.plot(np.sin(arc), np.cos(arc), 'k-', lw=2)
        ax1.plot([0, 0], [0, 1], 'k-', lw=2)
        ax1.plot([0, 1], [0, 0], 'k-', lw=2)
        ax1.set_aspect('equal')
        ax1.set_xlim(-0.05, 1.1)
        ax1.set_ylim(-0.05, 1.1)
        ax1.set_xlabel('r = sqrt(x²+y²)')
        ax1.set_ylabel('z = cosθ')
        ax1.set_title('Сечение полусферы: точек больше около оси N')
        ax1.grid(True, alpha=0.3)

        ax2.plot(theta_centers, density_hat, 'o-', lw=2, ms=5, label='оценка по выборке')
        ax2.plot(theta_centers, density_theory, '-', lw=3, label='теория: cosθ/π')
        ax2.set_xlabel('θ, рад')
        ax2.set_ylabel('Плотность по телесному углу')
        ax2.set_title('Проверка косинусного закона')
        ax2.legend()
        ax2.grid(True, alpha=0.3)

        mean_cos = float(np.mean(pts[:, 2]))
        self.set_info(
            'Алгоритм генерации:\n'
            '1) Берём r1, r2 ~ U(0,1), φ = 2πr1.\n'
            '2) Строим локальный вектор: x = sqrt(1-r2)cosφ, y = sqrt(1-r2)sinφ, z = sqrt(r2).\n'
            '3) Получаем плотность p(ω) = cosθ/π относительно нормали N.\n\n'
            'Как работает доказательство сейчас:\n'
            'Здесь надо доказать уже не равномерность, а именно косинусный закон. Поэтому используется другая идея: '
            'мы оцениваем плотность по телесному углу в угловых полосах и сравниваем её с теоретической кривой cosθ/π. '
            'Кроме того, на сечении полусферы видно, что около оси N точек больше, а к краю полусферы их становится меньше. '
            'Именно такое убывание и должно быть у косинусного распределения.\n\n'
            f'Среднее значение cosθ по выборке = {mean_cos:.6f}, теория = 2/3 ≈ 0.666667.'
        )
        self.canvas.draw()


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title('ЛР3 — графические доказательства распределений')
        self.geometry('1360x900')

        top = ttk.Frame(self)
        top.pack(side=tk.TOP, fill=tk.X, padx=8, pady=8)
        ttk.Label(
            top,
            text='Треугольник и круг — через окрестности; сфера — через телесные углы; косинусное — через сравнение плотности с теорией',
        ).pack(side=tk.LEFT)
        ttk.Button(top, text='О программе', command=self.show_about).pack(side=tk.RIGHT)

        notebook = ttk.Notebook(self)
        notebook.pack(fill=tk.BOTH, expand=True)
        notebook.add(TriangleTab(notebook), text='Треугольник')
        notebook.add(DiskTab(notebook), text='Круг')
        notebook.add(SphereTab(notebook), text='Сфера')
        notebook.add(CosineTab(notebook), text='Косинусное')

    @staticmethod
    def show_about() -> None:
        messagebox.showinfo(
            'О программе',
            'Программа генерирует по 100000 точек/направлений и показывает графическое доказательство:\n'
            '• треугольник и круг — через одинаковые окрестности;\n'
            '• сфера — через одинаковые телесные углы;\n'
            '• косинусное распределение — через сравнение оценки плотности с cos(θ)/π.'
        )


if __name__ == '__main__':
    app = App()
    app.mainloop()
