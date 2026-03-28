package org.example.task3;

import java.util.Objects;

public final class BookContent {
  private final String id; // удобно для тестов и ссылок в решениях
  private final ContentType type;
  private final String text;

  public BookContent(String id, ContentType type, String text) {
    if (id == null || id.isBlank())
      throw new IllegalArgumentException("id");
    if (type == null)
      throw new IllegalArgumentException("type");
    if (text == null || text.isBlank())
      throw new IllegalArgumentException("text");
    this.id = id;
    this.type = type;
    this.text = text;
  }

  public String id() {
    return id;
  }

  public ContentType type() {
    return type;
  }

  public String text() {
    return text;
  }

  @Override
  public boolean equals(Object o) {
    if (this == o)
      return true;
    if (!(o instanceof BookContent))
      return false;
    BookContent that = (BookContent) o;
    return id.equals(that.id);
  }

  @Override
  public int hashCode() {
    return Objects.hash(id);
  }

  @Override
  public String toString() {
    return "BookContent{id='" + id + "', type=" + type + "}";
  }
}
