package org.example.task3;

import java.util.*;

public final class Book {
  private final String title;
  private final List<BookContent> contents = new ArrayList<>();
  private final List<EditorDecision> decisions = new ArrayList<>();
  private final Map<String, BookContent> byId = new HashMap<>();

  public Book(String title) {
    if (title == null || title.isBlank())
      throw new IllegalArgumentException("title");
    this.title = title;
  }

  public String title() {
    return title;
  }

  public void addContent(BookContent content) {
    if (content == null)
      throw new IllegalArgumentException("content");
    if (byId.containsKey(content.id())) {
      throw new IllegalArgumentException("Duplicate content id: " + content.id());
    }
    contents.add(content);
    byId.put(content.id(), content);
  }

  public void addDecision(EditorDecision decision) {
    if (decision == null)
      throw new IllegalArgumentException("decision");
    // Решение должно ссылаться на реально существующий контент книги
    if (!byId.containsKey(decision.contentId())) {
      throw new IllegalArgumentException("Decision refers to unknown contentId: " + decision.contentId());
    }
    decisions.add(decision);
  }

  public List<BookContent> allContents() {
    return Collections.unmodifiableList(contents);
  }

  public List<EditorDecision> allDecisions() {
    return Collections.unmodifiableList(decisions);
  }

  /** "Неоднородная книга" — если присутствует более одного типа контента. */
  public boolean isHeterogeneous() {
    return contents.stream().map(BookContent::type).distinct().count() > 1;
  }

  /** Контент, который реально включен (по решениям редактора). */
  public List<BookContent> includedContents() {
    // если решений несколько на один контент — берём последнее по добавлению
    Map<String, EditorDecision> last = new HashMap<>();
    for (EditorDecision d : decisions)
      last.put(d.contentId(), d);

    List<BookContent> res = new ArrayList<>();
    for (BookContent c : contents) {
      EditorDecision d = last.get(c.id());
      if (d != null && d.isIncluded())
        res.add(c);
    }
    return Collections.unmodifiableList(res);
  }

  public Optional<EditorDecision> lastDecisionFor(String contentId) {
    if (contentId == null || contentId.isBlank())
      throw new IllegalArgumentException("contentId");
    for (int i = decisions.size() - 1; i >= 0; i--) {
      EditorDecision d = decisions.get(i);
      if (d.contentId().equals(contentId))
        return Optional.of(d);
    }
    return Optional.empty();
  }
}
