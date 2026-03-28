package org.example.task2;

import java.util.*;

public class Graph {
  // Чтобы обход был детерминированным: храним соседей в TreeSet (сортировка)
  private final Map<Integer, NavigableSet<Integer>> adj = new HashMap<>();

  public void addVertex(int v) {
    adj.computeIfAbsent(v, k -> new TreeSet<>());
  }

  public void addEdge(int a, int b) {
    if (a == b) {
      addVertex(a);
      adj.get(a).add(b);
      return;
    }
    addVertex(a);
    addVertex(b);
    adj.get(a).add(b);
    adj.get(b).add(a);
  }

  public NavigableSet<Integer> neighbors(int v) {
    return adj.getOrDefault(v, new TreeSet<>());
  }

  public Set<Integer> vertices() {
    return adj.keySet();
  }
}
