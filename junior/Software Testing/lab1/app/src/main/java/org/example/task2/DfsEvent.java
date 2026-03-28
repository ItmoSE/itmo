package org.example.task2;

import java.util.Objects;

public final class DfsEvent {
  public final DfsEventType type;
  public final int from;
  public final int to;

  private DfsEvent(DfsEventType type, int from, int to) {
    this.type = type;
    this.from = from;
    this.to = to;
  }

  public static DfsEvent enter(int v) {
    return new DfsEvent(DfsEventType.ENTER_VERTEX, v, -1);
  }

  public static DfsEvent exit(int v) {
    return new DfsEvent(DfsEventType.EXIT_VERTEX, v, -1);
  }

  public static DfsEvent consider(int from, int to) {
    return new DfsEvent(DfsEventType.CONSIDER_EDGE, from, to);
  }

  public static DfsEvent tree(int from, int to) {
    return new DfsEvent(DfsEventType.TREE_EDGE, from, to);
  }

  public static DfsEvent back(int from, int to) {
    return new DfsEvent(DfsEventType.BACK_EDGE, from, to);
  }

  @Override
  public boolean equals(Object o) {
    if (this == o)
      return true;
    if (!(o instanceof DfsEvent))
      return false;
    DfsEvent that = (DfsEvent) o;
    return from == that.from && to == that.to && type == that.type;
  }

  @Override
  public int hashCode() {
    return Objects.hash(type, from, to);
  }

  @Override
  public String toString() {
    if (to == -1)
      return type + "(" + from + ")";
    return type + "(" + from + "->" + to + ")";
  }
}
