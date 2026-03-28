package org.example.task3;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

public class BookDomainModelTest {

  // ---------- helpers ----------
  private static Book sampleBook() {
    Book b = new Book("\"Путеводитель по Галактике для автостопщиков\"");
    b.addContent(new BookContent("n1", ContentType.NARRATIVE, "Основной сюжетный фрагмент"));
    b.addContent(new BookContent("t1", ContentType.TRIVIA, "Занимательная вставка"));
    return b;
  }

  // =========================================================
  // БАЗОВАЯ ФУНКЦИОНАЛЬНОСТЬ
  // =========================================================

  @Test
  @DisplayName("Book: title() returns the title passed to constructor")
  void bookStoresAndReturnsTitle() {
    Book book = new Book("My Book");
    assertEquals("My Book", book.title());
  }

  @Test
  @DisplayName("Editor: name() returns the name passed to constructor")
  void editorStoresAndReturnsName() {
    Editor editor = new Editor("Ford Prefect");
    assertEquals("Ford Prefect", editor.name());
  }

  @Test
  @DisplayName("BookContent: getters return values passed to constructor")
  void bookContentStoresAndReturnsFields() {
    BookContent content = new BookContent(
        "c42",
        ContentType.TRIVIA,
        "42 — ответ на главный вопрос жизни, вселенной и всего такого");

    assertEquals("c42", content.id());
    assertEquals(ContentType.TRIVIA, content.type());
    assertEquals("42 — ответ на главный вопрос жизни, вселенной и всего такого", content.text());
  }

  @Test
  @DisplayName("EditorDecision: getters return values passed to constructor")
  void editorDecisionStoresAndReturnsFields() {
    Editor editor = new Editor("Slartibartfast");
    EditorDecision decision = new EditorDecision(
        editor,
        "t1",
        DecisionKind.INCLUDE,
        InterestLevel.INTERESTING,
        DiscoverySource.CAUGHT_EYE,
        "Очень занятный фрагмент");

    assertEquals(editor, decision.editor());
    assertEquals("t1", decision.contentId());
    assertEquals(DecisionKind.INCLUDE, decision.kind());
    assertEquals(InterestLevel.INTERESTING, decision.interest());
    assertEquals(DiscoverySource.CAUGHT_EYE, decision.discoverySource());
    assertEquals("Очень занятный фрагмент", decision.comment());
    assertTrue(decision.isIncluded());
  }

  @Test
  @DisplayName("EditorDecision.isIncluded: true for INCLUDE and false for EXCLUDE")
  void editorDecisionIsIncludedBehavior() {
    Editor editor = new Editor("Arthur Dent");

    EditorDecision include = new EditorDecision(
        editor, "c1", DecisionKind.INCLUDE, InterestLevel.OK, DiscoverySource.PLANNED_REVIEW, "");
    EditorDecision exclude = new EditorDecision(
        editor, "c1", DecisionKind.EXCLUDE, InterestLevel.BORING, DiscoverySource.PLANNED_REVIEW, "");

    assertTrue(include.isIncluded());
    assertFalse(exclude.isIncluded());
  }

  @Test
  @DisplayName("EditorDecision: stores discovery source")
  void editorDecisionStoresDiscoverySource() {
    Editor editor = new Editor("Редактор");

    EditorDecision decision = new EditorDecision(
        editor,
        "t1",
        DecisionKind.INCLUDE,
        InterestLevel.INTERESTING,
        DiscoverySource.CAUGHT_EYE,
        "Заметил случайно");

    assertEquals(DiscoverySource.CAUGHT_EYE, decision.discoverySource());
  }

  // =========================================================
  // ВАЛИДАЦИЯ И ИНВАРИАНТЫ
  // =========================================================

  @Test
  @DisplayName("Editor: name validation")
  void editorValidation() {
    assertThrows(IllegalArgumentException.class, () -> new Editor(null));
    assertThrows(IllegalArgumentException.class, () -> new Editor(""));
    assertThrows(IllegalArgumentException.class, () -> new Editor("   "));
    assertDoesNotThrow(() -> new Editor("Douglas"));
  }

  @Test
  @DisplayName("BookContent: validates id/type/text")
  void contentValidation() {
    assertThrows(IllegalArgumentException.class,
        () -> new BookContent(null, ContentType.NARRATIVE, "x"));
    assertThrows(IllegalArgumentException.class,
        () -> new BookContent("", ContentType.NARRATIVE, "x"));
    assertThrows(IllegalArgumentException.class,
        () -> new BookContent("   ", ContentType.NARRATIVE, "x"));

    assertThrows(IllegalArgumentException.class,
        () -> new BookContent("id", null, "x"));

    assertThrows(IllegalArgumentException.class,
        () -> new BookContent("id", ContentType.NARRATIVE, null));
    assertThrows(IllegalArgumentException.class,
        () -> new BookContent("id", ContentType.NARRATIVE, ""));
    assertThrows(IllegalArgumentException.class,
        () -> new BookContent("id", ContentType.NARRATIVE, "   "));

    assertDoesNotThrow(() -> new BookContent("id", ContentType.TRIVIA, "fact"));
  }

  @Test
  @DisplayName("BookContent: equality is based on id only")
  void contentEqualityById() {
    BookContent a = new BookContent("x", ContentType.NARRATIVE, "aaa");
    BookContent b = new BookContent("x", ContentType.TRIVIA, "bbb");
    assertEquals(a, b);
    assertEquals(a.hashCode(), b.hashCode());
  }

  @Test
  @DisplayName("Book: title validation and null content/decision validation")
  void bookValidation() {
    assertThrows(IllegalArgumentException.class, () -> new Book(null));
    assertThrows(IllegalArgumentException.class, () -> new Book(""));
    assertThrows(IllegalArgumentException.class, () -> new Book("   "));

    Book book = new Book("Title");
    assertThrows(IllegalArgumentException.class, () -> book.addContent(null));
    assertThrows(IllegalArgumentException.class, () -> book.addDecision(null));
  }

  @Test
  @DisplayName("Book: cannot add duplicate content id")
  void bookNoDuplicateContentId() {
    Book book = new Book("Title");
    book.addContent(new BookContent("c1", ContentType.NARRATIVE, "a"));
    assertThrows(IllegalArgumentException.class,
        () -> book.addContent(new BookContent("c1", ContentType.NARRATIVE, "b")));
  }

  @Test
  @DisplayName("Book: decision must refer to existing contentId")
  void decisionMustReferToExistingContent() {
    Book book = new Book("Title");
    book.addContent(new BookContent("c1", ContentType.NARRATIVE, "a"));

    Editor ed = new Editor("Editor");
    EditorDecision bad = new EditorDecision(
        ed, "unknown", DecisionKind.INCLUDE, InterestLevel.INTERESTING, DiscoverySource.PLANNED_REVIEW, "ok");

    assertThrows(IllegalArgumentException.class, () -> book.addDecision(bad));
  }

  @Test
  @DisplayName("EditorDecision: validates fields + invariant INCLUDE cannot be BORING")
  void editorDecisionValidationAndInvariant() {
    Editor ed = new Editor("Editor");

    assertThrows(IllegalArgumentException.class,
        () -> new EditorDecision(null, "c1", DecisionKind.INCLUDE, InterestLevel.INTERESTING,
            DiscoverySource.PLANNED_REVIEW, ""));
    assertThrows(IllegalArgumentException.class,
        () -> new EditorDecision(ed, null, DecisionKind.INCLUDE, InterestLevel.INTERESTING,
            DiscoverySource.PLANNED_REVIEW, ""));
    assertThrows(IllegalArgumentException.class,
        () -> new EditorDecision(ed, "", DecisionKind.INCLUDE, InterestLevel.INTERESTING,
            DiscoverySource.PLANNED_REVIEW, ""));
    assertThrows(IllegalArgumentException.class,
        () -> new EditorDecision(ed, "c1", null, InterestLevel.INTERESTING,
            DiscoverySource.PLANNED_REVIEW, ""));
    assertThrows(IllegalArgumentException.class,
        () -> new EditorDecision(ed, "c1", DecisionKind.INCLUDE, null,
            DiscoverySource.PLANNED_REVIEW, ""));
    assertThrows(IllegalArgumentException.class,
        () -> new EditorDecision(ed, "c1", DecisionKind.INCLUDE, InterestLevel.INTERESTING,
            null, ""));

    assertThrows(IllegalArgumentException.class,
        () -> new EditorDecision(ed, "c1", DecisionKind.INCLUDE, InterestLevel.BORING,
            DiscoverySource.PLANNED_REVIEW, "no"));

    assertDoesNotThrow(() -> new EditorDecision(
        ed, "c1", DecisionKind.INCLUDE, InterestLevel.INTERESTING,
        DiscoverySource.PLANNED_REVIEW, "yes"));

    assertDoesNotThrow(() -> new EditorDecision(
        ed, "c1", DecisionKind.EXCLUDE, InterestLevel.BORING,
        DiscoverySource.CAUGHT_EYE, "ok to exclude"));
  }

  // =========================================================
  // ПОВЕДЕНИЕ Book
  // =========================================================

  @Test
  @DisplayName("Book.isHeterogeneous: false for 0/1 distinct content types")
  void heterogeneityFalseForHomogeneous() {
    Book empty = new Book("Empty");
    assertFalse(empty.isHeterogeneous());

    Book onlyNarrative = new Book("N");
    onlyNarrative.addContent(new BookContent("n1", ContentType.NARRATIVE, "a"));
    onlyNarrative.addContent(new BookContent("n2", ContentType.NARRATIVE, "b"));
    assertFalse(onlyNarrative.isHeterogeneous());

    Book onlyTrivia = new Book("T");
    onlyTrivia.addContent(new BookContent("t1", ContentType.TRIVIA, "a"));
    assertFalse(onlyTrivia.isHeterogeneous());
  }

  @Test
  @DisplayName("Book.isHeterogeneous: true when there are at least 2 content types")
  void heterogeneityTrueForMixed() {
    Book b = sampleBook();
    assertTrue(b.isHeterogeneous());
  }

  @Test
  @DisplayName("Book.includedContents: returns only content with last decision INCLUDE")
  void includedContentsRespectsLastDecision() {
    Book b = sampleBook();
    Editor ed = new Editor("Editor");

    b.addDecision(new EditorDecision(
        ed, "n1", DecisionKind.INCLUDE, InterestLevel.OK, DiscoverySource.PLANNED_REVIEW, ""));
    b.addDecision(new EditorDecision(
        ed, "t1", DecisionKind.INCLUDE, InterestLevel.INTERESTING, DiscoverySource.CAUGHT_EYE, ""));

    assertEquals(List.of("n1", "t1"),
        b.includedContents().stream().map(BookContent::id).toList());

    b.addDecision(new EditorDecision(
        ed, "t1", DecisionKind.EXCLUDE, InterestLevel.BORING, DiscoverySource.PLANNED_REVIEW, "remove"));

    assertEquals(List.of("n1"),
        b.includedContents().stream().map(BookContent::id).toList());
  }

  @Test
  @DisplayName("Book.lastDecisionFor: empty when none, returns last when exists")
  void lastDecisionForBehavior() {
    Book b = sampleBook();
    Editor ed = new Editor("Editor");

    assertTrue(b.lastDecisionFor("n1").isEmpty());

    EditorDecision d1 = new EditorDecision(
        ed, "n1", DecisionKind.INCLUDE, InterestLevel.OK, DiscoverySource.PLANNED_REVIEW, "v1");
    EditorDecision d2 = new EditorDecision(
        ed, "n1", DecisionKind.EXCLUDE, InterestLevel.BORING, DiscoverySource.CAUGHT_EYE, "v2");

    b.addDecision(d1);
    b.addDecision(d2);

    assertEquals(d2, b.lastDecisionFor("n1").orElseThrow());
  }

  @Test
  @DisplayName("Book.lastDecisionFor: validates contentId input")
  void lastDecisionForValidation() {
    Book b = sampleBook();
    assertThrows(IllegalArgumentException.class, () -> b.lastDecisionFor(null));
    assertThrows(IllegalArgumentException.class, () -> b.lastDecisionFor(""));
    assertThrows(IllegalArgumentException.class, () -> b.lastDecisionFor("   "));
  }

  @Test
  @DisplayName("Book exposes unmodifiable lists for contents/decisions")
  void unmodifiableViews() {
    Book b = sampleBook();
    Editor ed = new Editor("Editor");
    b.addDecision(new EditorDecision(
        ed, "n1", DecisionKind.INCLUDE, InterestLevel.OK, DiscoverySource.PLANNED_REVIEW, ""));

    assertThrows(UnsupportedOperationException.class, () -> b.allContents().add(
        new BookContent("x", ContentType.NARRATIVE, "x")));

    assertThrows(UnsupportedOperationException.class, () -> b.allDecisions().add(
        new EditorDecision(ed, "n1", DecisionKind.EXCLUDE, InterestLevel.BORING,
            DiscoverySource.CAUGHT_EYE, "x")));
  }

  // =========================================================
  // ПОЛНАЯ ПРЕДМЕТНАЯ ОБЛАСТЬ
  // =========================================================

  @Test
  @DisplayName("Полная предметная область: редактор включает занимательную вставку, которая попалась ему на глаза")
  void fullDomainScenario() {
    Book book = new Book("Путеводитель по Галактике для автостопщиков");

    BookContent narrative = new BookContent(
        "n1",
        ContentType.NARRATIVE,
        "Описание путешествий по Галактике");

    BookContent trivia = new BookContent(
        "t1",
        ContentType.TRIVIA,
        "Полотенце — самая полезная вещь для автостопщика");

    book.addContent(narrative);
    book.addContent(trivia);

    Editor editor = new Editor("Редактор");

    EditorDecision decision = new EditorDecision(
        editor,
        "t1",
        DecisionKind.INCLUDE,
        InterestLevel.INTERESTING,
        DiscoverySource.CAUGHT_EYE,
        "Показалось занимательным");

    book.addDecision(decision);

    assertTrue(book.isHeterogeneous());
    assertEquals(DiscoverySource.CAUGHT_EYE, decision.discoverySource());
    assertEquals(InterestLevel.INTERESTING, decision.interest());
    assertEquals(decision, book.lastDecisionFor("t1").orElseThrow());

    List<BookContent> included = book.includedContents();
    assertEquals(1, included.size());
    assertEquals("t1", included.get(0).id());
    assertEquals(ContentType.TRIVIA, included.get(0).type());

    assertEquals("Путеводитель по Галактике для автостопщиков", book.title());
    assertEquals("Редактор", editor.name());
    assertEquals(DecisionKind.INCLUDE, decision.kind());
  }
}
