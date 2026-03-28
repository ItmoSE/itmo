package org.example.task3;

import java.util.Objects;

public final class Editor {
  private final String name;

  public Editor(String name) {
    if (name == null || name.isBlank())
      throw new IllegalArgumentException("name");
    this.name = name;
  }

  public String name() {
    return name;
  }

  @Override
  public boolean equals(Object o) {
    if (this == o)
      return true;
    if (!(o instanceof Editor))
      return false;
    Editor editor = (Editor) o;
    return name.equals(editor.name);
  }

  @Override
  public int hashCode() {
    return Objects.hash(name);
  }

  @Override
  public String toString() {
    return "Editor{name='" + name + "'}";
  }
}
