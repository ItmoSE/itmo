package org.example.task3;

import java.util.Objects;

public final class EditorDecision {
  private final Editor editor;
  private final String contentId;
  private final DecisionKind kind;
  private final InterestLevel interest;
  private final DiscoverySource discoverySource; // как редактор столкнулся с материалом
  private final String comment;

  public EditorDecision(Editor editor,
      String contentId,
      DecisionKind kind,
      InterestLevel interest,
      DiscoverySource discoverySource,
      String comment) {
    if (editor == null)
      throw new IllegalArgumentException("editor");
    if (contentId == null || contentId.isBlank())
      throw new IllegalArgumentException("contentId");
    if (kind == null)
      throw new IllegalArgumentException("kind");
    if (interest == null)
      throw new IllegalArgumentException("interest");
    if (discoverySource == null)
      throw new IllegalArgumentException("discoverySource");

    if (kind == DecisionKind.INCLUDE && interest == InterestLevel.BORING) {
      throw new IllegalArgumentException("Cannot INCLUDE BORING content");
    }

    this.editor = editor;
    this.contentId = contentId;
    this.kind = kind;
    this.interest = interest;
    this.discoverySource = discoverySource;
    this.comment = (comment == null) ? "" : comment;
  }

  public Editor editor() {
    return editor;
  }

  public String contentId() {
    return contentId;
  }

  public DecisionKind kind() {
    return kind;
  }

  public InterestLevel interest() {
    return interest;
  }

  public DiscoverySource discoverySource() {
    return discoverySource;
  }

  public String comment() {
    return comment;
  }

  public boolean isIncluded() {
    return kind == DecisionKind.INCLUDE;
  }

  @Override
  public boolean equals(Object o) {
    if (this == o)
      return true;
    if (!(o instanceof EditorDecision))
      return false;
    EditorDecision that = (EditorDecision) o;
    return editor.equals(that.editor)
        && contentId.equals(that.contentId)
        && kind == that.kind
        && interest == that.interest
        && discoverySource == that.discoverySource
        && comment.equals(that.comment);
  }

  @Override
  public int hashCode() {
    return Objects.hash(editor, contentId, kind, interest, discoverySource, comment);
  }

  @Override
  public String toString() {
    return "EditorDecision{editor=" + editor +
        ", contentId='" + contentId + '\'' +
        ", kind=" + kind +
        ", interest=" + interest +
        ", discoverySource=" + discoverySource +
        '}';
  }
}
