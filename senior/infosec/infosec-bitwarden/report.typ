#let markdown-export = sys.inputs.at("x-target", default: "pdf") == "md"
#show: body => {
  if markdown-export {
    body
  } else {
    set page(
      paper: "a4",
      margin: (x: 20mm, y: 18mm),
      footer: align(center)[г. Санкт-Петербург 2026],
    )
    body
  }
}
#set text(size: 10pt)
#set par(justify: true, leading: 0.7em)

#let terminal(body) = block(
  width: 100%,
  fill: rgb("e1e5ea"),
  stroke: (left: 2pt + rgb("66788a")),
  inset: (x: 9pt, y: 7pt),
  radius: 3pt,
  breakable: true,
)[#body]
#show raw.where(block: true): set text(size: 8pt)

// Универсальный шаблон для скриншота.
// Меняйте только path, title и caption.
#let screenshot(path, title, caption) = block(
  width: 100%,
  breakable: false,
  above: 0.4cm,
  below: 0.5cm,
)[
  // Заголовок входит в ту же фигуру, что и уменьшенное изображение.
  #figure(
    block(width: 100%, breakable: false)[
      #align(left, text(weight: "bold", title))
      #v(0.2cm)
      #align(center, image(path, width: 75%))
    ],
    kind: image,
    caption: [#caption],
    placement: none,
  )
]

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
  #text(weight: "bold")[По лабораторной работе N5]
  #linebreak()
  по дисциплине Информационная Безопасность
  #linebreak()
  #text(weight: "bold")[Вариант: -]
]

#v(6.9cm)

#align(right)[
  Работу выполнил:
  #linebreak()
  Молчанов Федор Денисович P3413
  #linebreak()
  #linebreak()
  Работу принял:
  #linebreak()
  Маркина Татьяна Анатольевна
]

#v(2cm)

#pagebreak()
#show: body => {
  if markdown-export {
    body
  } else {
    set page(
      footer: context {
        align(center)[#counter(page).display("1")]
      },
    )
    counter(page).update(1)
    body
  }
}

= 1 - Bitwarden

У меня установлен Bitwarden в качестве менеджера паролей, в виде extension в браузере и как мобильное приложение на телефоне.

#screenshot(
  "images/1-interface.png",
  "Интерфейс Bitwarden",
  "С уже существующими записями",
)

= 2 - Оценка паролей через https://haveibeenpwned.com/

Я проверял свои два самых часто используемых мейла: `@mail.ru`, `@gmail.com`

#screenshot(
  "images/2-mail-breaches.png",
  "Breaches для первого мейла",
  "Обнаружено 3 штуки",
)

#screenshot(
  "images/3-breach-1.png",
  "Первая утечка",
  "Synthient",
)

#screenshot(
  "images/4-breach-2.png",
  "Вторая утчека",
  "CDEK",
)

#screenshot(
  "images/5-breach-3.png",
  "Третья утчека",
  "Alpine Replay",
)

#screenshot(
  "images/6-gmail-breaches.png",
  "Gmail breaches",
  "Одна находка",
)

#screenshot(
  "images/7-gmail-breach-1.png",
  "Единственная утечка",
  "Suno",
)


= Замена паролей

Я вручную прошёлся по всем паролям и посмотрел, есть ли те, которые я не заменял на автогенерируемые. Оказалось, что такие есть.


#screenshot(
  "images/8-asus.png",
  "Asus",
  "Вход по старому",
)

#screenshot(
  "images/9-asus-new.png",
  "Замена пароля на новый",
  "Видно, что размер пароля больше",
)

#screenshot(
  "images/10-bethesda.jpg",
  "Bethesda",
  "Доказательство изменения",
)

#screenshot(
  "images/11-epicgames.png",
  "Epic Games",
  "Доказательство изменения",
)

Также был изменен еще один пароль, но подтверждения об этом на почту не было прислано.

= 2 Factor Authentication

- Была произведена с помощью сервиса Authy

Так как скриншот из Authy сделать нельзя, покажу, что у меня действительно есть 2FA на GH:

#screenshot(
  "images/12-FA.png",
  "2FA на Github",
  "Она существует!)",
)

#pagebreak()

= Краткие ответы на вопросы

- Какие пароли были слабыми? Все пароли, которые я заменял, были интерпретацией моего старого пароля, который я ошибочно использовал на многих аккаунтах (добавляя в конец 1-2 цифры, или изменяя регистр букв). Пароль состоял из 9 символов без цифр или спец. символов. Что делает его весьма ненадёжным.

- 2-факторная аутентификация используется для улучшения защиты аккаунта на случай, когда исходный пароль от аккаунта был получен злоумышленником. В таком случае о не сможет сразу же получить доступ к аккаунту - ему еще потребуется пройти 2-факторную аутентификацию, что зачастую невозможно без самого владельца аккаунта.

