#set page(
  paper: "a4",
  margin: (x: 20mm, y: 18mm),
  footer: align(center)[г. Санкт-Петербург 2026],
)
#set text(size: 10pt)
#set par(justify: true, leading: 0.7em)

// Единое оформление листингов. Для блоков с языком bash и dockerfile Typst
// выполняет встроенную подсветку синтаксиса.
#let terminal(body) = block(
  width: 100%,
  fill: rgb("e1e5ea"),
  stroke: (left: 2pt + rgb("66788a")),
  inset: (x: 9pt, y: 7pt),
  radius: 3pt,
  breakable: true,
)[#body]
#show raw.where(block: true): set text(size: 8pt)

#v(1.6cm)

#align(center)[
  Федеральное государственное автономное образовательное
  #linebreak()
  учреждение высшего образования
  #linebreak()
  «Национальный исследовательский университет ИТМО»
]

#v(3.7cm)

#align(center)[
  #text(weight: "bold")[Отчёт]
  #linebreak()
  #text(weight: "bold")[По лабораторной работе N2]
  #linebreak()
  по дисциплине Администрирование систем и сетей
  #linebreak()
  #text(weight: "bold")[Вариант: -]
  #linebreak()
  #text(weight: "bold")[Желаемая оценка: 4]
]

#v(6.9cm)

#align(right)[
  Работу выполнили:
  #linebreak()
  Молчанов Федор Денисович P3413
  #linebreak()
  Пышкин Никита Сергеевич P3413
  #linebreak()
  #linebreak()
  Работу принял:
  #linebreak()
  Максимов Андрей Николаевич
]

#v(2cm)

#pagebreak()
#set page(
  footer: context {
    align(center)[
      #counter(page).display("1")
    ]
  },
)
#counter(page).update(1)

#outline(title: [Оглавление], depth: 3, indent: 1.35em)
#pagebreak()
