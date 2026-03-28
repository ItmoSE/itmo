package org.example;

public class Func {

  private static final double DEFAULT_EPS = 1e-12;

  public static double arccos(double x) {
    return arccos(x, Integer.MAX_VALUE, DEFAULT_EPS);
  }

  public static double arccos(double x, int maxTerms) {
    return arccos(x, maxTerms, DEFAULT_EPS);
  }

  public static double arccos(double x, double eps) {
    return arccos(x, Integer.MAX_VALUE, eps);
  }

  /**
   * Реализация arccos(x) через степенной ряд.
   *
   * @param x        аргумент из диапазона [-1, 1]
   * @param maxTerms максимальное число добавляемых членов ряда
   * @param eps      требуемая точность остановки: если очередной член ряда по
   *                 модулю <= eps,
   *                 вычисление прекращается
   * @return значение arccos(x)
   */
  public static double arccos(double x, int maxTerms, double eps) {
    if (Double.isNaN(x) || Math.abs(x) > 1.0) {
      return Double.NaN;
    }

    if (!Double.isFinite(eps) || eps <= 0.0) {
      throw new IllegalArgumentException("eps must be positive and finite");
    }

    if (maxTerms < 0) {
      throw new IllegalArgumentException("maxTerms must be >= 0");
    }

    if (x == -0d || x == 0d) {
      return Math.PI / 2.0;
    }

    if (x == 1.0) {
      return 0.0;
    }

    if (x == -1.0) {
      return Math.PI;
    }

    // Улучшение сходимости около концов отрезка [-1, 1]
    if (Math.abs(x) >= 0.999999) {
      if (x > 0) {
        double t = Math.sqrt((1.0 - x) / 2.0);
        return 2.0 * arcsinSeries(t, maxTerms, eps);
      } else {
        double t = Math.sqrt((1.0 + x) / 2.0);
        return Math.PI - 2.0 * arcsinSeries(t, maxTerms, eps);
      }
    }

    // arccos(x) = PI/2 - arcsin(x)
    return (Math.PI / 2.0) - arcsinSeries(x, maxTerms, eps);
  }

  /**
   * arcsin(x) через степенной ряд:
   * arcsin(x) = x + x^3/6 + 3x^5/40 + ...
   *
   * @param x        аргумент из [-1, 1]
   * @param maxTerms максимальное число добавляемых членов после первого x
   * @param eps      порог остановки
   * @return значение arcsin(x)
   */
  private static double arcsinSeries(double x, int maxTerms, double eps) {
    if (Double.isNaN(x) || Math.abs(x) > 1.0) {
      return Double.NaN;
    }

    if (x == -0d || x == 0d) {
      return x;
    }

    double sum = x;
    double term = x;

    for (int k = 0; k < maxTerms; k++) {
      double multiplier = ((2.0 * k + 1.0) * (2.0 * k + 1.0) * x * x) /
          (2.0 * (k + 1.0) * (2.0 * k + 3.0));

      term *= multiplier;
      sum += term;

      if (Math.abs(term) <= eps) {
        break;
      }
    }

    return sum;
  }
}
