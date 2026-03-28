package org.example.task2;

import java.util.*;

public class DepthFirstSearch {

  /**
   * DFS для неориентированного графа.
   *
   * Характерные точки (events):
   * - ENTER_VERTEX(v)
   * - CONSIDER_EDGE(v->to)
   * - TREE_EDGE(v->to) + рекурсивный вызов
   * - BACK_EDGE(v->to) (если to уже visited и to != parent[v])
   * - EXIT_VERTEX(v)
   */
  public static List<DfsEvent> dfs(Graph g, int start) {
    List<DfsEvent> events = new ArrayList<>();
    Set<Integer> visited = new HashSet<>();
    Map<Integer, Integer> parent = new HashMap<>();

    parent.put(start, -1);
    dfsRec(g, start, visited, parent, events);

    return events;
  }

  private static void dfsRec(
      Graph g,
      int v,
      Set<Integer> visited,
      Map<Integer, Integer> parent,
      List<DfsEvent> events) {
    visited.add(v);
    events.add(DfsEvent.enter(v));

    for (int to : g.neighbors(v)) {
      events.add(DfsEvent.consider(v, to));

      if (!visited.contains(to)) {
        parent.put(to, v);
        events.add(DfsEvent.tree(v, to));
        dfsRec(g, to, visited, parent, events);
      } else {
        int p = parent.getOrDefault(v, -1);
        // Для неориентированного графа ребро в parent — это "обратное ребро" того же
        // ребра,
        // обычно его не считают BACK_EDGE, поэтому исключаем.
        if (to != p) {
          events.add(DfsEvent.back(v, to));
        }
      }
    }

    events.add(DfsEvent.exit(v));
  }
}
