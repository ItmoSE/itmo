package org.example;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.ValueSource;

public class FuncTest {

  @ParameterizedTest(name = "arccos({0})")
  @DisplayName("Compare with Math.acos on representative values")
  @ValueSource(doubles = {
      -999.9,
      -1.0000001,
      -1.0,
      -0.999999999999,
      -0.99,
      -0.5,
      -0.000001,
      -0.0001,
      -0.0,
      0.0,
      0.0001,
      0.000001,
      0.5,
      0.99,
      0.999999999999,
      1.0,
      1.0000001,
      999.9,
      Double.NaN,
      Double.POSITIVE_INFINITY,
      Double.NEGATIVE_INFINITY,
      Double.MIN_VALUE,
  })
  void checkRepresentativeValues(double param) {
    double expected = Math.acos(param);
    double actual = Func.arccos(param);

    if (Double.isNaN(expected)) {
      assertTrue(Double.isNaN(actual));
    } else {
      assertEquals(expected, actual, 1e-10);
    }
  }

  @Test
  @DisplayName("Check -0.0 behavior explicitly")
  void checkNegativeZero() {
    double x = -0.0;
    assertEquals(Math.acos(x), Func.arccos(x), 0.0);
    assertEquals(Math.PI / 2.0, Func.arccos(x), 0.0);
  }

  @Test
  @DisplayName("Custom eps gives acceptable accuracy")
  void checkCustomEpsAccuracy() {
    double x = 0.75;
    double eps = 1e-8;

    double expected = Math.acos(x);
    double actual = Func.arccos(x, 10_000, eps);

    assertEquals(expected, actual, 1e-8);
  }

  @Test
  @DisplayName("Smaller eps should usually produce more accurate result")
  void checkSmallerEpsImprovesAccuracy() {
    double x = 0.99;
    double expected = Math.acos(x);

    double rough = Func.arccos(x, 10_000, 1e-3);
    double precise = Func.arccos(x, 10_000, 1e-12);

    double roughError = Math.abs(expected - rough);
    double preciseError = Math.abs(expected - precise);

    assertTrue(preciseError <= roughError);
    assertTrue(preciseError < 1e-10);
  }

  @Test
  @DisplayName("Limited number of terms reduces accuracy")
  void checkMaxTermsAffectsAccuracy() {
    double x = 0.8;
    double expected = Math.acos(x);

    double fewTerms = Func.arccos(x, 1, 1e-16);
    double manyTerms = Func.arccos(x, 10_000, 1e-16);

    double fewTermsError = Math.abs(expected - fewTerms);
    double manyTermsError = Math.abs(expected - manyTerms);

    assertTrue(manyTermsError < fewTermsError);
  }

  @Test
  @DisplayName("Invalid eps should throw exception")
  void checkInvalidEps() {
    assertThrows(IllegalArgumentException.class, () -> Func.arccos(0.5, 100, 0.0));
    assertThrows(IllegalArgumentException.class, () -> Func.arccos(0.5, 100, -1e-6));
    assertThrows(IllegalArgumentException.class, () -> Func.arccos(0.5, 100, Double.NaN));
    assertThrows(IllegalArgumentException.class, () -> Func.arccos(0.5, 100, Double.POSITIVE_INFINITY));
  }

  @Test
  @DisplayName("Invalid maxTerms should throw exception")
  void checkInvalidMaxTerms() {
    assertThrows(IllegalArgumentException.class, () -> Func.arccos(0.5, -1, 1e-8));
  }

  @Test
  @DisplayName("Exact endpoint values")
  void checkExactEndpoints() {
    assertEquals(0.0, Func.arccos(1.0), 0.0);
    assertEquals(Math.PI, Func.arccos(-1.0), 0.0);
  }
}
