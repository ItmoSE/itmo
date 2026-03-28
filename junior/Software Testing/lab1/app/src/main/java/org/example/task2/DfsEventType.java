package org.example.task2;

public enum DfsEventType {
  ENTER_VERTEX, // вошли в вершину (пометили visited)
  CONSIDER_EDGE, // рассматриваем ребро (v -> to)
  TREE_EDGE, // ребро в непосещенную вершину (переход/рекурсия)
  BACK_EDGE, // ребро в уже посещенную (не parent) вершину
  EXIT_VERTEX // выходим из вершины (после обработки всех соседей)
}
