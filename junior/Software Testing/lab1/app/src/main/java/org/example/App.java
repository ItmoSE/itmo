package org.example;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;

import org.example.task3.*;
import org.example.task2.*;

public class App {

  public static void main(String[] args) {
    if (args.length == 0) {
      printHelp();
      return;
    }

    switch (args[0].toLowerCase()) {
      case "dfs" -> runDfsDemo();
      case "task3" -> runTask3Demo();
      default -> {
        System.out.println("Unknown command: " + args[0]);
        printHelp();
      }
    }
  }

  private static void printHelp() {
    System.out.println("Usage:");
    System.out.println("  ./gradlew run --args=\"dfs\"     # DFS demo (task2)");
    System.out.println("  ./gradlew run --args=\"task3\"   # Domain model demo (task3)");
  }

  // -------------------- TASK2: DFS demo --------------------
  private static void runDfsDemo() {
    System.out.println("=== Task2: DFS demo ===");

    // 1--2
    // | /|
    // |/ |
    // 3--4
    // \
    // 5
    Graph g = new Graph();
    g.addEdge(1, 2);
    g.addEdge(1, 3);
    g.addEdge(2, 3);
    g.addEdge(2, 4);
    g.addEdge(3, 4);
    g.addEdge(3, 5);

    int start = 1;

    printAsciiPicture();
    printGraphAsAdjacencyList(g);
    printEdges(g);

    List<DfsEvent> events = DepthFirstSearch.dfs(g, start);

    System.out.println("\n=== DFS run ===");
    System.out.println("Start vertex: " + start);

    List<Integer> enterOrder = events.stream()
        .filter(e -> e.type == DfsEventType.ENTER_VERTEX)
        .map(e -> e.from)
        .toList();
    System.out.println("Enter order (discovery order): " + enterOrder);

    System.out.println("\nEvent trace (characteristic points):");
    for (int i = 0; i < events.size(); i++) {
      System.out.printf("%3d) %s%n", i + 1, events.get(i));
    }

    long treeEdges = events.stream().filter(e -> e.type == DfsEventType.TREE_EDGE).count();
    long backEdges = events.stream().filter(e -> e.type == DfsEventType.BACK_EDGE).count();
    System.out.println("\nSummary:");
    System.out.println("TREE_EDGE count = " + treeEdges);
    System.out.println("BACK_EDGE count = " + backEdges);
  }

  private static void printAsciiPicture() {
    System.out.println("\n=== Graph picture (schematic) ===");
    System.out.println("   1--2");
    System.out.println("   | /|");
    System.out.println("   |/ |");
    System.out.println("   3--4");
    System.out.println("    \\");
    System.out.println("     5");
  }

  private static void printGraphAsAdjacencyList(Graph g) {
    System.out.println("\n=== Adjacency list (sorted neighbors) ===");
    List<Integer> verts = new ArrayList<>(g.vertices());
    Collections.sort(verts);

    for (int v : verts) {
      String neigh = g.neighbors(v).stream()
          .map(String::valueOf)
          .collect(Collectors.joining(", "));
      System.out.printf("%d: [%s]%n", v, neigh);
    }
  }

  private static void printEdges(Graph g) {
    System.out.println("\n=== Edge list (undirected) ===");
    List<Integer> verts = new ArrayList<>(g.vertices());
    Collections.sort(verts);

    for (int u : verts) {
      for (int v : g.neighbors(u)) {
        if (u < v)
          System.out.printf("(%d - %d)%n", u, v);
      }
    }
  }

  // -------------------- TASK3: Domain model demo --------------------
  private static void runTask3Demo() {
    System.out.println("=== Task3: Book domain model demo ===");

    Book book = new Book("\"Путеводитель по Галактике для автостопщиков\"");

    book.addContent(new BookContent("n1", ContentType.NARRATIVE, "Основной сюжетный фрагмент"));
    book.addContent(new BookContent("t1", ContentType.TRIVIA, "Случайная занимательная вставка"));
    book.addContent(new BookContent("t2", ContentType.TRIVIA, "Ещё один факт, который может быть интересен"));

    Editor editor = new Editor("Editor-1");

    book.addDecision(new EditorDecision(
        editor,
        "n1",
        DecisionKind.INCLUDE,
        InterestLevel.OK,
        DiscoverySource.PLANNED_REVIEW,
        "основа книги"));

    book.addDecision(new EditorDecision(
        editor,
        "t1",
        DecisionKind.INCLUDE,
        InterestLevel.INTERESTING,
        DiscoverySource.CAUGHT_EYE,
        "попалась на глаза и показалась занимательной"));

    book.addDecision(new EditorDecision(
        editor,
        "t2",
        DecisionKind.EXCLUDE,
        InterestLevel.INTERESTING,
        DiscoverySource.CAUGHT_EYE,
        "интересно, но не подходит по тону"));

    // редактор передумал по t1
    book.addDecision(new EditorDecision(
        editor,
        "t1",
        DecisionKind.EXCLUDE,
        InterestLevel.OK,
        DiscoverySource.PLANNED_REVIEW,
        "слишком выбивается"));

    System.out.println("Title: " + book.title());
    System.out.println("Is heterogeneous: " + book.isHeterogeneous());

    System.out.println("\nAll contents:");
    for (BookContent c : book.allContents()) {
      System.out.printf(" - %s [%s]: %s%n", c.id(), c.type(), c.text());
    }

    System.out.println("\nAll editor decisions (in order):");
    for (EditorDecision d : book.allDecisions()) {
      System.out.printf(
          " - contentId=%s | editor=%s | kind=%s | interest=%s | discovery=%s | comment='%s'%n",
          d.contentId(),
          d.editor().name(),
          d.kind(),
          d.interest(),
          d.discoverySource(),
          d.comment());
    }

    System.out.println("\nLast decision per content:");
    for (BookContent c : book.allContents()) {
      var last = book.lastDecisionFor(c.id());
      if (last.isPresent()) {
        EditorDecision d = last.get();
        System.out.printf(
            " - %s: %s, interest=%s, discovery=%s, comment='%s'%n",
            c.id(),
            d.kind(),
            d.interest(),
            d.discoverySource(),
            d.comment());
      } else {
        System.out.printf(" - %s: <none>%n", c.id());
      }
    }

    System.out.println("\nIncluded contents (by last decision INCLUDE):");
    List<BookContent> included = book.includedContents();
    if (included.isEmpty()) {
      System.out.println(" <none>");
    } else {
      for (BookContent c : included) {
        System.out.printf(" - %s [%s]: %s%n", c.id(), c.type(), c.text());
      }
    }
  }
}
