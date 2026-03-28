package org.example.task2;

import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.assertEquals;

public class DepthFirstSearchTest {

  @Test
  @DisplayName("DFS: simple chain 1-2-3")
  void dfsChain() {
    Graph g = new Graph();
    g.addEdge(1, 2);
    g.addEdge(2, 3);

    List<DfsEvent> actual = DepthFirstSearch.dfs(g, 1);

    List<DfsEvent> expected = List.of(
        DfsEvent.enter(1),
        DfsEvent.consider(1, 2),
        DfsEvent.tree(1, 2),

        DfsEvent.enter(2),
        DfsEvent.consider(2, 1), // parent -> не back
        DfsEvent.consider(2, 3),
        DfsEvent.tree(2, 3),

        DfsEvent.enter(3),
        DfsEvent.consider(3, 2), // parent -> не back
        DfsEvent.exit(3),

        DfsEvent.exit(2),
        DfsEvent.exit(1));

    assertEquals(expected, actual);
  }

  @Test
  @DisplayName("DFS: triangle 1-2-3-1 (cycle)")
  void dfsTriangleCycle() {
    Graph g = new Graph();
    g.addEdge(1, 2);
    g.addEdge(2, 3);
    g.addEdge(1, 3);

    List<DfsEvent> actual = DepthFirstSearch.dfs(g, 1);

    List<DfsEvent> expected = List.of(
        DfsEvent.enter(1),

        DfsEvent.consider(1, 2),
        DfsEvent.tree(1, 2),

        DfsEvent.enter(2),
        DfsEvent.consider(2, 1), // parent
        DfsEvent.consider(2, 3),
        DfsEvent.tree(2, 3),

        DfsEvent.enter(3),
        DfsEvent.consider(3, 1),
        DfsEvent.back(3, 1), // 1 visited и не parent(3)=2 => back edge
        DfsEvent.consider(3, 2), // parent
        DfsEvent.exit(3),

        DfsEvent.exit(2),

        DfsEvent.consider(1, 3),
        DfsEvent.back(1, 3), // 3 уже visited и не parent(1)=-1 => back edge

        DfsEvent.exit(1));

    assertEquals(expected, actual);
  }

  @Test
  @DisplayName("DFS: star graph center 1 connected to 2,3,4")
  void dfsStar() {
    Graph g = new Graph();
    g.addEdge(1, 2);
    g.addEdge(1, 3);
    g.addEdge(1, 4);

    List<DfsEvent> actual = DepthFirstSearch.dfs(g, 1);

    List<DfsEvent> expected = List.of(
        DfsEvent.enter(1),

        DfsEvent.consider(1, 2),
        DfsEvent.tree(1, 2),
        DfsEvent.enter(2),
        DfsEvent.consider(2, 1), // parent
        DfsEvent.exit(2),

        DfsEvent.consider(1, 3),
        DfsEvent.tree(1, 3),
        DfsEvent.enter(3),
        DfsEvent.consider(3, 1), // parent
        DfsEvent.exit(3),

        DfsEvent.consider(1, 4),
        DfsEvent.tree(1, 4),
        DfsEvent.enter(4),
        DfsEvent.consider(4, 1), // parent
        DfsEvent.exit(4),

        DfsEvent.exit(1));

    assertEquals(expected, actual);
  }

  @Test
  @DisplayName("DFS: self-loop at 1 and edge 1-2")
  void dfsSelfLoop() {
    Graph g = new Graph();
    g.addEdge(1, 1); // петля
    g.addEdge(1, 2);

    List<DfsEvent> actual = DepthFirstSearch.dfs(g, 1);

    List<DfsEvent> expected = List.of(
        DfsEvent.enter(1),

        DfsEvent.consider(1, 1),
        DfsEvent.back(1, 1), // visited и не parent => back (сам в себя)

        DfsEvent.consider(1, 2),
        DfsEvent.tree(1, 2),
        DfsEvent.enter(2),
        DfsEvent.consider(2, 1), // parent
        DfsEvent.exit(2),

        DfsEvent.exit(1));

    assertEquals(expected, actual);
  }

  @Test
  @DisplayName("DFS: single vertex graph")
  void dfsSingleVertex() {
    Graph g = new Graph();
    g.addVertex(1);

    List<DfsEvent> actual = DepthFirstSearch.dfs(g, 1);

    List<DfsEvent> expected = List.of(
        DfsEvent.enter(1),
        DfsEvent.exit(1));

    assertEquals(expected, actual);
  }

  @Test
  @DisplayName("DFS: disconnected graph visits only start component")
  void dfsDisconnectedGraph() {
    Graph g = new Graph();

    // Компонента 1: 1-2-3
    g.addEdge(1, 2);
    g.addEdge(2, 3);

    // Компонента 2: 10-11
    g.addEdge(10, 11);

    List<DfsEvent> actual = DepthFirstSearch.dfs(g, 1);

    List<DfsEvent> expected = List.of(
        DfsEvent.enter(1),
        DfsEvent.consider(1, 2),
        DfsEvent.tree(1, 2),

        DfsEvent.enter(2),
        DfsEvent.consider(2, 1), // parent
        DfsEvent.consider(2, 3),
        DfsEvent.tree(2, 3),

        DfsEvent.enter(3),
        DfsEvent.consider(3, 2), // parent
        DfsEvent.exit(3),

        DfsEvent.exit(2),
        DfsEvent.exit(1));

    assertEquals(expected, actual);
  }
}
