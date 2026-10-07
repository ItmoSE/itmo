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
  #text(weight: "bold")[По лабораторной работе N4]
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

#set text(size: 10pt, lang: "ru")
#set par(justify: false, leading: 0.55em, spacing: 0.7em)
#show heading: set block(above: 1em, below: 0.55em)
#show heading.where(level: 1): set text(size: 14pt)
#show heading.where(level: 2): set text(size: 12pt)
#set figure(gap: 4pt)
#show figure.caption: set text(size: 8pt, fill: rgb("505967"))

#let separator() = {
  v(4pt)
  line(length: 100%, stroke: 0.5pt + rgb("c9ced5"))
  v(2pt)
}

// Показываем нужный фрагмент исходного скриншота средствами вёрстки.
#let screenshot(path, caption, height: 26mm, offset: 0mm) = block(breakable: false)[
  #block(width: 100%, height: height, clip: true)[
    #move(dy: -offset)[#image(path, width: 100%)]
  ]
  #text(size: 8pt, fill: rgb("505967"))[#caption]
]

#let assessment(risk, confidence) = block(
  fill: rgb("f1f3f6"),
  inset: (x: 7pt, y: 5pt),
  width: 100%,
)[*Оценка ZAP:* риск — #risk; уверенность — #confidence.]

#outline(title: [Оглавление], depth: 2)
#pagebreak()

= Сканирование

*Стенд:* DVWA в Docker, `http://127.0.0.1:4280/`, уровень `low`.
*Инструмент:* ZAP 2.17.0, Arch Linux; дата — 07.10.2026.

Через браузер ZAP создана база и выполнен обход разделов. Quick Scan
завершился, но SQLi и XSS появились после отправки форм с `id=1` и `name=test`
и отдельных Active Scan этих запросов (495 и 309 запросов).
Ниже разобраны три типа предупреждений только для DVWA.

= Анализ находок

== 1. SQL-инъекция

#assessment([высокий], [средняя])

*Что обнаружено.* В `/vulnerabilities/sqli/` передана кавычка в параметре `id`.
Сервер вернул `Uncaught mysqli_sql_exception` и `You have an error in your SQL syntax`.
Ошибка воспроизведена вручную в браузере.

*Почему опасно.* Ответ раскрывает СУБД MariaDB, вызов `mysqli_query`, путь
к обработчику и строку 11. Это помогает понять устройство приложения и
подобрать дальнейшие проверки. Путь сам по себе не даёт доступ к файлу.
Главная угроза — возможность изменить смысл SQL-запроса: расширить выборку
и получить чужие записи. Изменение данных или обход входа зависят от запроса
и прав пользователя БД.

*Вывод.* Ошибка — сильный признак SQLi и доказанная утечка диагностических
сведений. Чтение и изменение данных через SQLi в работе не проверялись.
Приоритет исправления высокий.

*Исправление.* Параметризованные запросы, проверка типа `id`, минимальные
права БД. Подробные ошибки сохранять в журнале [1].

#screenshot(
  "screenshots/06-sqli-details.png",
  [SQL Injection — MySQL: параметр id, тестовая кавычка и Evidence.],
  height: 25mm,
)
#figure(image("screenshots/13-sqli-browser-error.png", width: 100%), caption: [Та же SQL-ошибка при ручной проверке.])

#separator()

== 2. Отражённый XSS

#assessment([высокий], [средняя; исполнение проверено вручную])

*Что обнаружено.* В параметр `name` страницы `/vulnerabilities/xss_r/`
передана строка `</pre><script>alert(1);</scRipt><pre>`.
ZAP обнаружил её отражение; при открытии ссылки браузер показал диалог `1`.

*Почему опасно.* Сам alert безвреден, но доказывает, что ввод стал исполняемым
JavaScript. Вместо него код мог бы читать данные страницы, подменять формы
и отправлять запросы с полномочиями пользователя. Для атаки нужна жертва,
открывшая подготовленную ссылку. HttpOnly ограничивает чтение cookie,
но не мешает коду менять страницу и выполнять запросы.

*Вывод.* Выполнение JavaScript подтверждено. Потенциальный ущерб высокий,
но доступ к ОС, другим сайтам или всем данным сервера из этого не следует.

*Исправление.* Кодировать вывод согласно HTML-контексту; вставлять обычный
текст без интерпретации HTML. CSP использовать как дополнительную защиту [2].

#screenshot(
  "screenshots/09-xss-reflected-details.png",
  [Cross Site Scripting (Reflected): тестовая строка отражена в Evidence.],
)

#screenshot(
  "screenshots/12-xss-browser-execution.png",
  [Ручная проверка: браузер исполнил alert(1) на localhost:4280.],
)

== 3. Отсутствие защиты от clickjacking

#assessment([средний], [средняя])

*Что обнаружено.* Для `/setup.php` ZAP сообщает Missing Anti-clickjacking
Header: отсутствуют `X-Frame-Options` и защита через CSP `frame-ancestors`.
Это пассивная проверка заголовков, поэтому тестовой строки в Attack нет.

*Почему опасно.* Если страницу можно встроить в iframe, злоумышленник может
скрыть её под обманным интерфейсом. Пользователь думает, что нажимает одну
кнопку, а фактически нажимает кнопку приложения. На странице настройки DVWA
такое действие — сброс базы.

*Ручная проверка.* В гостевой книге DVWA сохранена контрольная запись
`clickjacking-check-02` от имени `test`. Затем на странице
`http://localhost:8000/clickjacking-check.html` в iframe открыта
`http://127.0.0.1:4280/setup.php`. Внешняя страница и DVWA имеют разные origin.
Кнопка `Create / Reset Database` совмещена с приманкой «Забрать подарок»:
сначала при частичной прозрачности для проверки положения, затем iframe
сделан полностью прозрачным. Клик по приманке попал на настоящую кнопку
внутри iframe и отправил форму DVWA.

После клика DVWA показала `Database has been created.` и `Setup successful!`,
а также сообщения о создании таблиц и заполнении исходными данными.
При последующем открытии гостевой книги контрольная запись отсутствовала;
осталась стандартная запись `This is a test comment.`. Это подтверждает
сброс к исходному состоянию, а не полное опустошение базы.

*Вывод.* На учебном стенде вручную воспроизведён clickjacking со сбросом БД.
Подтверждены встраивание с другого origin, подмена видимого интерфейса
и потеря контрольной записи после клика. В конфигурации стенда включено
`DISABLE_AUTHENTICATION: "true"`: использование авторизованной сессии жертвы
в этом опыте не проверялось. Ущерб ограничен данными учебного стенда.

*Исправление.* `Content-Security-Policy: frame-ancestors 'none'` либо
`X-Frame-Options: DENY`, если встраивание не требуется [3].

#screenshot(
  "screenshots/10-anti-clickjacking-details.png",
  [Missing Anti-clickjacking Header для setup.php.],
  height: 33mm,
)

#figure(
  image("screenshots/14-clickjacking-before.png", width: 100%),
  caption: [До опыта: в гостевой книге сохранена контрольная запись clickjacking-check-02.],
)

#figure(
  image("screenshots/15-clickjacking-overlay.png", width: 100%),
  caption: [Настройка совмещения: сквозь iframe видна настоящая кнопка Create / Reset Database поверх приманки.],
)

#figure(
  image("screenshots/16-clickjacking-bait.png", width: 100%),
  caption: [Подмена включена: iframe полностью прозрачен, пользователю видна кнопка «Забрать подарок».],
)

#figure(
  image("screenshots/17-clickjacking-reset-result.png", width: 80%),
  caption: [Ответ DVWA после клика: база и таблицы созданы заново, Setup successful!],
)

#figure(
  image("screenshots/18-clickjacking-after.png", width: 100%),
  caption: [После сброса: контрольная запись исчезла, в гостевой книге осталась стандартная запись.],
)

#separator()

== Приоритеты

#table(
  columns: (1.1fr, 2.3fr),
  inset: 5pt,
  stroke: 0.4pt + rgb("d3d7dd"),
  fill: (x, y) => if y == 0 { rgb("eef1f5") } else { none },
  table.header([*Находка*], [*Оценка по результатам работы*]),
  [SQLi], [Высокий приоритет: воспроизводимая SQL-ошибка; доступ к данным не проверен.],
  [Reflected XSS], [Высокий приоритет: исполнение JavaScript подтверждено.],
  [Clickjacking], [Средний риск по ZAP; вручную подтверждён сброс учебной БД через подмену интерфейса.],
)

Дополнительный DOM Based XSS оставлен для проверки: в предоставленных
подробностях Evidence пусто, механизм обработки данных в DOM не установлен.
Он не включён в число подтверждённых находок.

== Контрольные вопросы

*DAST и SAST.* DAST проверяет работающее приложение запросами; SAST
анализирует исходный код без запуска.

*Последствия XSS.* Подмена страницы, чтение доступных данных и действия
в приложении с полномочиями пользователя.

*Тестирование коммерческих сайтов.* Активные проверки требуют разрешения
владельца и согласованных границ: они могут нарушить работу и данные.

== Источники

#text()[
  #show link: set text(fill: rgb("0563c1"))
  #show link: underline

  [1] #link("https://cheatsheetseries.owasp.org/cheatsheets/SQL_Injection_Prevention_Cheat_Sheet.html")[OWASP: предотвращение SQLi].
  #linebreak()
  [2] #link("https://cheatsheetseries.owasp.org/cheatsheets/Cross_Site_Scripting_Prevention_Cheat_Sheet.html")[OWASP: предотвращение XSS].
  #linebreak()
  [3] #link("https://www.zaproxy.org/docs/alerts/10020-1/")[ZAP: Missing Anti-clickjacking Header].

]
