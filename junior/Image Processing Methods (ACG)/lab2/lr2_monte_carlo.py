
import math
from dataclasses import dataclass
from typing import Callable, Dict, List, Tuple

import numpy as np

'''
В лабораторной я вычисляю интеграл методом Монте-Карло, заменяя интеграл на среднее значение функции в случайных точках.
Далее я улучшаю точность разными методами: стратификацией, важностной выборкой, MIS и русской рулеткой.
И сравниваю полученные результаты с аналитическим значением.
'''

A = 2.0
B = 5.0
SAMPLE_SIZES = [100, 1000, 10000, 100000]
ROULETTE_THRESHOLDS = [0.5, 0.75, 0.95]


def f(x: np.ndarray) -> np.ndarray:
    return x ** 2


def true_integral(a: float = A, b: float = B) -> float:
    return (b ** 3 - a ** 3) / 3.0


def uniform_sample(n: int, a: float = A, b: float = B, rng: np.random.Generator | None = None) -> np.ndarray:
    if rng is None:
        rng = np.random.default_rng()
    return rng.uniform(a, b, size=n)


def mc_simple(n: int, rng: np.random.Generator) -> Tuple[float, float]:
    x = uniform_sample(n, rng=rng)
    values = (B - A) * f(x)
    return values.mean(), values.std(ddof=1) / math.sqrt(n)


def make_poly_pdf(power: int, a: float = A, b: float = B) -> Tuple[
    Callable[[np.ndarray], np.ndarray],
    Callable[[int, np.random.Generator], np.ndarray],
    str
]:
    # Формируется плотность вероятности p(x) для метода Монте-Карло с выборкой по значимости.
    #
    # По заданию:
    # "Использовать три варианта плотности вероятности p(x)"
    #
    # Здесь реализуется общий случай:
    # p(x) ∝ x^k  →  p(x) = C * x^k
    #
    # Нормировочная константа C находится из условия:
    # ∫_a^b p(x) dx = 1
    #
    # ⇒ C * ∫_a^b x^k dx = 1
    # ⇒ C * (b^(k+1) - a^(k+1)) / (k+1) = 1
    #
    # ⇒ C = (k+1) / (b^(k+1) - a^(k+1))

    coef = (power + 1) / (b ** (power + 1) - a ** (power + 1))

    def pdf(x: np.ndarray) -> np.ndarray:
        # Плотность вероятности:
        # p(x) = (k+1) / (b^(k+1) - a^(k+1)) * x^k
        return coef * (x ** power)

    def sampler(n: int, rng: np.random.Generator) -> np.ndarray:
        # Генерация выборки по p(x) методом обратной функции распределения (inverse transform sampling)
        #
        # F(x) = ∫_a^x p(t) dt
        #      = (x^(k+1) - a^(k+1)) / (b^(k+1) - a^(k+1))
        #
        # Генерируем u ~ U(0,1), приравниваем:
        # u = F(x)
        #
        # ⇒ u = (x^(k+1) - a^(k+1)) / (b^(k+1) - a^(k+1))
        #
        # ⇒ x^(k+1) = u * (b^(k+1) - a^(k+1)) + a^(k+1)
        #
        # ⇒ x = [u * (b^(k+1) - a^(k+1)) + a^(k+1)]^(1/(k+1))

        u = rng.random(n)
        return (u * (b ** (power + 1) - a ** (power + 1)) + a ** (power + 1)) ** (1.0 / (power + 1))

    # Возвращаем:
    # pdf — функция плотности p(x)
    # sampler — генератор случайных величин по этой плотности
    # строку для отчета (какая плотность используется)
    return pdf, sampler, f"p(x) ∝ x^{power}"


def mc_importance(
    n: int,
    pdf: Callable[[np.ndarray], np.ndarray],
    sampler: Callable[[int, np.random.Generator], np.ndarray],
    rng: np.random.Generator,
) -> Tuple[float, float]:
    x = sampler(n, rng)
    values = f(x) / pdf(x)
    return values.mean(), values.std(ddof=1) / math.sqrt(n)


def mc_stratified(n: int, step: float, rng: np.random.Generator) -> Tuple[float, float]:
    # Делит [A, B] на страты и усредняет вклад каждой страты отдельно.
    edges = np.arange(A, B + step, step)
    if edges[-1] < B:
        edges = np.append(edges, B)

    widths = np.diff(edges)
    m = len(widths)

    base = n // m
    rem = n % m
    counts = np.full(m, base, dtype=int)
    counts[:rem] += 1

    values_all = []
    estimate = 0.0

    for i in range(m):
        left, right = edges[i], edges[i + 1]
        ni = counts[i]
        x = rng.uniform(left, right, size=ni)
        contrib = widths[i] * f(x)
        estimate += contrib.mean()
        values_all.append(contrib)

    values_concat = np.concatenate(values_all)
    return estimate, values_concat.std(ddof=1) / math.sqrt(n)


def multiple_importance_sampling(
    n: int,
    pdf1: Callable[[np.ndarray], np.ndarray],
    sampler1: Callable[[int, np.random.Generator], np.ndarray],
    pdf2: Callable[[np.ndarray], np.ndarray],
    sampler2: Callable[[int, np.random.Generator], np.ndarray],
    rng: np.random.Generator,
    squared_weights: bool = False,
) -> Tuple[float, float]:
    # MIS: смешивает две плотности и задает веса по balance/power heuristic.
    n1 = n // 2
    n2 = n - n1

    x1 = sampler1(n1, rng)
    x2 = sampler2(n2, rng)

    p1_x1 = pdf1(x1)
    p2_x1 = pdf2(x1)
    p1_x2 = pdf1(x2)
    p2_x2 = pdf2(x2)

    if not squared_weights:
        w1 = p1_x1 / (p1_x1 + p2_x1)
        w2 = p2_x2 / (p1_x2 + p2_x2)
    else:
        w1 = (p1_x1 ** 2) / (p1_x1 ** 2 + p2_x1 ** 2)
        w2 = (p2_x2 ** 2) / (p1_x2 ** 2 + p2_x2 ** 2)

    est1 = np.sum(w1 * f(x1) / p1_x1) / n1
    est2 = np.sum(w2 * f(x2) / p2_x2) / n2
    estimate = est1 + est2

    all_terms = np.concatenate([
        w1 * f(x1) / p1_x1,
        w2 * f(x2) / p2_x2,
    ])
    return estimate, all_terms.std(ddof=1) / math.sqrt(n)


def mc_russian_roulette(n: int, threshold: float, rng: np.random.Generator) -> Tuple[float, float]:
    # Здесь русская рулетка реализована как отсечение малых вкладов с компенсацией веса.
    # Порог threshold задается для нормированной функции на [A, B].
    x = uniform_sample(n, rng=rng)
    fx = f(x)
    fmin = f(np.array([A]))[0]
    fmax = f(np.array([B]))[0]

    norm_fx = (fx - fmin) / (fmax - fmin)
    survival_prob = np.where(norm_fx >= threshold, 1.0,
                             np.clip(norm_fx / threshold, 1e-12, 1.0))
    survive = rng.random(n) < survival_prob

    contrib = np.zeros(n)
    contrib[survive] = (B - A) * fx[survive] / survival_prob[survive]
    return contrib.mean(), contrib.std(ddof=1) / math.sqrt(n)


@dataclass
class ResultRow:
    method: str
    n: int
    estimate: float
    abs_error: float
    est_error: float


def append_result(rows: List[ResultRow], method: str, n: int, estimate: float, est_error: float, exact: float) -> None:
    rows.append(
        ResultRow(
            method=method,
            n=n,
            estimate=estimate,
            abs_error=abs(estimate - exact),
            est_error=est_error,
        )
    )


def print_table(rows: List[ResultRow], exact: float) -> None:
    print(f"Истинный интеграл: {exact:.10f}\n")
    header = f"{'Метод':42} {'N':>8} {'Оценка интеграла':>18} {'|Ошибка|':>14} {'Оценка погр.':>14}"
    print(header)
    print("-" * len(header))
    for row in rows:
        print(
            f"{row.method:42} "
            f"{row.n:8d} "
            f"{row.estimate:18.10f} "
            f"{row.abs_error:14.10f} "
            f"{row.est_error:14.10f}"
        )


def main() -> None:
    rng = np.random.default_rng()
    exact = true_integral()
    rows: List[ResultRow] = []

    p_x, s_x, name_x = make_poly_pdf(1)
    p_x2, s_x2, name_x2 = make_poly_pdf(2)
    p_x3, s_x3, name_x3 = make_poly_pdf(3)

    for n in SAMPLE_SIZES:
        est, err = mc_simple(n, rng)
        append_result(rows, "Простой Monte-Carlo", n, est, err, exact)

        est, err = mc_stratified(n, step=1.0, rng=rng)
        append_result(rows, "Стратификация, шаг 1.0", n, est, err, exact)

        est, err = mc_stratified(n, step=0.5, rng=rng)
        append_result(rows, "Стратификация, шаг 0.5", n, est, err, exact)

        for pdf, sampler, name in [(p_x, s_x, name_x), (p_x2, s_x2, name_x2), (p_x3, s_x3, name_x3)]:
            est, err = mc_importance(n, pdf, sampler, rng)
            append_result(
                rows, f"Важностная выборка: {name}", n, est, err, exact)

        est, err = multiple_importance_sampling(
            n, p_x, s_x, p_x3, s_x3, rng, squared_weights=False)
        append_result(rows, "MIS: средняя плотность", n, est, err, exact)

        est, err = multiple_importance_sampling(
            n, p_x, s_x, p_x3, s_x3, rng, squared_weights=True)
        append_result(rows, "MIS: средний квадрат плотностей",
                      n, est, err, exact)

        for threshold in ROULETTE_THRESHOLDS:
            est, err = mc_russian_roulette(n, threshold, rng)
            append_result(
                rows, f"Русская рулетка, R={threshold}", n, est, err, exact)

    print("Функция: f(x) = x^2")
    print(f"Интервал: [{A}, {B}]")
    print_table(rows, exact)


if __name__ == "__main__":
    main()
