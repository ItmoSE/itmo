from dataclasses import dataclass
import math
from typing import List, Tuple

'''
Мы берём точку на треугольнике, переводим её в глобальные координаты, считаем нормаль.
Затем для каждого источника считаем освещённость через угол между нормалью и светом и расстояние.
После этого применяем модель отражения (диффузную и зеркальную).
Если наблюдатель находится с обратной стороны поверхности, яркость обнуляется.
Итоговая яркость — сумма вкладов всех источников.

точка → нормаль → свет → освещенность → отражение → яркость
'''

EPS = 1e-9


@dataclass
class Vec3:
    x: float
    y: float
    z: float

    def __add__(self, other: "Vec3") -> "Vec3":
        return Vec3(self.x + other.x, self.y + other.y, self.z + other.z)

    def __sub__(self, other: "Vec3") -> "Vec3":
        return Vec3(self.x - other.x, self.y - other.y, self.z - other.z)

    def __mul__(self, scalar: float) -> "Vec3":
        return Vec3(self.x * scalar, self.y * scalar, self.z * scalar)

    def __rmul__(self, scalar: float) -> "Vec3":
        return self.__mul__(scalar)

    def __truediv__(self, scalar: float) -> "Vec3":
        if abs(scalar) < EPS:
            raise ZeroDivisionError("Division by zero in Vec3")
        return Vec3(self.x / scalar, self.y / scalar, self.z / scalar)

    def length(self) -> float:
        return math.sqrt(self.x * self.x + self.y * self.y + self.z * self.z)

    def normalized(self) -> "Vec3":
        l = self.length()
        if l < EPS:
            return Vec3(0.0, 0.0, 0.0)
        return self / l

    def clamp01(self) -> "Vec3":
        return Vec3(
            max(0.0, min(1.0, self.x)),
            max(0.0, min(1.0, self.y)),
            max(0.0, min(1.0, self.z)),
        )

    def __repr__(self) -> str:
        return f"({self.x:.6f}, {self.y:.6f}, {self.z:.6f})"


def dot(a: Vec3, b: Vec3) -> float:
    """
    Скалярное произведение.

    считает угол
    определяет освещённость
    проверяет видимость

    Что делает:
        Возвращает одно число:
            a.x*b.x + a.y*b.y + a.z*b.z

    Физический смысл:
        Показывает, насколько два вектора направлены в одну сторону.
        Через него вычисляется cos угла между векторами.

    Где используется:
        1. Проверка, освещена ли поверхность:
           dot(normal, light_dir)
        2. Проверка, видит ли наблюдатель лицевую сторону:
           dot(normal, view_dir)
        3. Зеркальная составляющая:
           dot(normal, h)

    Результат:
        > 0  -> угол острый, направления близки
        = 0  -> векторы перпендикулярны
        < 0  -> векторы смотрят в разные стороны
    """
    return a.x * b.x + a.y * b.y + a.z * b.z


def cross(a: Vec3, b: Vec3) -> Vec3:
    """
    Векторное произведение.

    Что делает:
        Возвращает новый вектор, перпендикулярный a и b.

    Где используется:
        Для вычисления нормали к плоскости треугольника.
    """
    return Vec3(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    )


def hadamard(a: Vec3, b: Vec3) -> Vec3:
    """
    Поэлементное (покомпонентное) умножение векторов.

    Что делает:
        Возвращает вектор:
            (a.x*b.x, a.y*b.y, a.z*b.z)

    Физический смысл:
        Используется как умножение цветов по каналам RGB.

    Где используется:
        1. Цвет света * цвет поверхности
        2. Цвет источника * коэффициент отражения

    Пример:
        light_color = (1, 0, 0)      # красный свет
        surface_color = (0, 1, 0)    # зелёная поверхность

        hadamard(...) = (0, 0, 0)

    То есть:
        красный свет не даёт зелёного отражения.
    """
    return Vec3(a.x * b.x, a.y * b.y, a.z * b.z)


@dataclass
class Light:
    position: Vec3
    intensity: Vec3   # RGB сила излучения
    direction: Vec3   # ось источника
    power: float = 1.0  # степень в диаграмме cos^power(theta)


@dataclass
class Material:
    surface_color: Vec3
    kd: float  # коэффициент диффузного отражения
    ks: float  # коэффициент зеркального отражения
    shininess: float  # степень блеска


@dataclass
class Triangle:
    p0: Vec3
    p1: Vec3
    p2: Vec3

    def point_from_local(self, u: float, v: float) -> Vec3:
        """
        Перевод локальных координат (u, v) в глобальные координаты точки
        на плоскости треугольника.

        Формула:
            P = P0 + u*(P1 - P0) + v*(P2 - P0)
        """
        return self.p0 + (self.p1 - self.p0) * u + (self.p2 - self.p0) * v

    def normal(self) -> Vec3:
        """
        Нормаль к плоскости треугольника.
        """
        e1 = self.p1 - self.p0
        e2 = self.p2 - self.p0
        return cross(e1, e2).normalized()


def light_emission_towards_point(light: Light, point: Vec3) -> Vec3:
    """
    'Цветная' сила излучения источника в направлении точки.

    Модель:
        I(theta) = I0 * max(0, cos(theta))^power

    Где:
        theta — угол между осью источника и направлением распространения света.
    """
    light_to_point = (point - light.position).normalized()
    axis = light.direction.normalized()
    cos_theta = max(0.0, dot(axis, light_to_point))
    factor = cos_theta ** light.power
    return light.intensity * factor


def illuminance_at_point(point: Vec3, normal: Vec3, light: Light) -> Vec3:
    """
    Освещенность точки от одного источника.

    Формула:
        E = I(theta) * max(0, cos(alpha)) / R^2

    Где:
        alpha — угол между направлением на источник и нормалью.
        R — расстояние от точки до источника.
    """
    to_light = light.position - point
    r = to_light.length()
    if r < EPS:
        return Vec3(0.0, 0.0, 0.0)

    light_dir = to_light / r
    cos_alpha = max(0.0, dot(normal, light_dir))
    if cos_alpha <= 0.0:
        return Vec3(0.0, 0.0, 0.0)

    emitted = light_emission_towards_point(light, point)
    return emitted * (cos_alpha / (r * r))


def brdf(normal: Vec3, light_dir: Vec3, view_dir: Vec3, material: Material) -> Tuple[float, float]:
    """
    BRDF в упрощенной модели:
        диффузная + зеркальная составляющая

    Диффузная:
        kd

    Зеркальная:
        ks * max(0, dot(normal, h))^shininess

    h — средний вектор между направлением света и наблюдения.
    """
    h = (light_dir + view_dir).normalized()

    diffuse = material.kd
    spec_angle = max(0.0, dot(normal, h))
    specular = material.ks * (spec_angle ** material.shininess)

    return diffuse, specular


def brightness_at_point(
    point: Vec3,
    normal: Vec3,
    observer: Vec3,
    lights: List[Light],
    material: Material,
) -> Vec3:
    """
    Итоговая яркость точки.

    Важное условие из задания:
    Если наблюдатель находится под телом и смотрит на темную сторону,
    яркости быть не должно.

    Это реализовано проверкой:
        dot(normal, view_dir) <= 0  => яркость = 0
    """
    view_dir = (observer - point).normalized()

    # Проверка: наблюдатель видит только лицевую сторону поверхности
    if dot(normal, view_dir) <= 0.0:
        return Vec3(0.0, 0.0, 0.0)

    result = Vec3(0.0, 0.0, 0.0)

    for light in lights:
        to_light = (light.position - point).normalized()
        e = illuminance_at_point(point, normal, light)

        if e.length() < EPS:
            continue

        diffuse_coeff, specular_coeff = brdf(
            normal, to_light, view_dir, material)

        diffuse_part = hadamard(e, material.surface_color) * diffuse_coeff
        specular_part = e * specular_coeff

        result = result + diffuse_part + specular_part

    return result.clamp01()


def print_case(
    case_name: str,
    triangle: Triangle,
    observer: Vec3,
    lights: List[Light],
    material: Material,
    test_points: List[Tuple[float, float]],
) -> None:
    print("=" * 80)
    print(case_name)
    print("=" * 80)

    n = triangle.normal()
    print(f"Normal: {n}")
    print(f"Observer: {observer}")
    print()

    for u, v in test_points:
        p = triangle.point_from_local(u, v)
        b = brightness_at_point(p, n, observer, lights, material)
        print(
            f"Point local (u={u:.2f}, v={v:.2f}) "
            f"-> global {p} "
            f"-> brightness {b}"
        )
    print()


def main():
    # Треугольник в плоскости z = 0
    triangle = Triangle(
        p0=Vec3(0.0, 0.0, 0.0),
        p1=Vec3(2.0, 0.0, 0.0),
        p2=Vec3(0.0, 2.0, 0.0),
    )

    # Материал поверхности
    material = Material(
        surface_color=Vec3(0.7, 0.7, 0.7),  # серый цвет поверхности
        kd=0.8,
        ks=0.3,
        shininess=20.0,
    )

    # Два источника света
    lights = [
        Light(
            position=Vec3(1.0, 1.0, 3.0),
            intensity=Vec3(8.0, 8.0, 8.0),
            direction=Vec3(0.0, 0.0, -1.0),
            power=1.0,
        ),
        Light(
            position=Vec3(-1.0, 1.5, 2.0),
            intensity=Vec3(4.0, 3.5, 3.5),
            direction=Vec3(1.0, -0.2, -1.0),
            power=2.0,
        ),
    ]

    # Больше тестовых точек в локальных координатах
    # Для корректного положения на треугольнике желательно u >= 0, v >= 0, u + v <= 1
    test_points = [
        (0.05, 0.05),
        (0.10, 0.10),
        (0.20, 0.10),
        (0.30, 0.20),
        (0.40, 0.10),
        (0.20, 0.30),
        (0.10, 0.50),
        (0.25, 0.25),
        (0.35, 0.15),
        (0.15, 0.35),
        (0.45, 0.05),
        (0.05, 0.45),
        (0.50, 0.20),
        (0.20, 0.60),
        (0.30, 0.30),
    ]

    # Наблюдатель сверху: должен видеть освещённую сторону
    observer_above = Vec3(1.0, 1.0, 4.0)

    # Наблюдатель снизу: должен видеть тёмную сторону
    # По условию задания яркости быть не должно
    observer_below = Vec3(1.0, 1.0, -4.0)

    print_case(
        case_name="CASE 1: Observer above the triangle (brightness should exist)",
        triangle=triangle,
        observer=observer_above,
        lights=lights,
        material=material,
        test_points=test_points,
    )

    print_case(
        case_name="CASE 2: Observer below the triangle (brightness should be zero)",
        triangle=triangle,
        observer=observer_below,
        lights=lights,
        material=material,
        test_points=test_points,
    )


if __name__ == "__main__":
    main()
