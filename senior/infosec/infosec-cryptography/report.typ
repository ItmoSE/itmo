#set page(
  paper: "a4",
  margin: (x: 20mm, y: 18mm),
  footer: align(center)[г. Санкт-Петербург 2026],
)
#set text(size: 10pt, lang: "ru")
#set par(justify: true, leading: 0.7em)
#show link: set text(fill: rgb("1565c0"))
#show link: underline

#let terminal(body) = block(
  width: 100%,
  fill: rgb("e1e5ea"),
  stroke: (left: 2pt + rgb("66788a")),
  inset: (x: 9pt, y: 7pt),
  radius: 3pt,
  breakable: true,
)[#body]
#show raw.where(block: true): it => terminal(it)
#show raw.where(block: true): set text(size: 8pt)
#show raw.where(block: true): set par(justify: false)
#show figure.where(kind: table): set text(size: 8pt)
#show table: set par(justify: false)

#let illustration(path, caption) = figure(
  image(path, width: 100%),
  caption: caption,
)

#let stride-category(title, english) = table.cell(
  colspan: 3,
  fill: rgb("e8eef5"),
)[
  *#title* #h(0.5em) #text(size: 8pt, fill: rgb("526174"))[#english]
]

// Титульный лист.

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
  #text(weight: "bold")[По лабораторной работе № 6]
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
  Работу приняла:
  #linebreak()
  Маркина Татьяна Анатольевна
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

= Цель работы

Получение практического опыта применения симметричного и асимметричного шифрования: создание зашифрованного файлового контейнера VeraCrypt, генерация пары OpenPGP-ключей и шифрование текстового сообщения.

= Ход работы

== Создание зашифрованного контейнера VeraCrypt

Была запущена программа VeraCrypt и создан стандартный зашифрованный файловый контейнер `lab6.hc` размером 100 MiB.

При создании контейнера были заданы следующие параметры:

- алгоритм шифрования — AES;
- хеш-функция — SHA-512;
- доступ к контейнеру — по паролю;
- PIM — не использовался;
- keyfiles — не использовались;
- файловая система — ext4;
- режим использования файловой системы — только Linux.

После создания контейнер был смонтирован в слот 1. Точка монтирования: `/run/media/veracrypt1`.

#illustration("veracrypt-mounted.png", [Смонтированный том VeraCrypt])

В смонтированном томе были созданы два тестовых файла. Ниже приведён полный вывод терминала при создании и проверке файлов.

```bash
➜  ~ cd /run/media/veracrypt1/
➜  veracrypt1 echo "secret  message for lab6" > secret.txt
➜  veracrypt1 echo "second encrypted" > second.txt
➜  veracrypt1 l
total 15K
drwxr-xr-x 3 theodor theodor 1.0K Oct  8 00:14 .
drwxr-xr-x 3 root    root      60 Oct  8 00:12 ..
drwx------ 2 root    root     12K Oct  8 00:10 lost+found
-rw-r--r-- 1 theodor theodor   17 Oct  8 00:14 second.txt
-rw-r--r-- 1 theodor theodor   25 Oct  8 00:14 secret.txt
```

Чистая проверка содержимого, до и после монтирования/демонтирования:
```bash
➜  ~ veracrypt ~/my-coding/school/itmo/senior/infosec/infosec-cryptography/lab6.hc /run/media/veracrypt1
➜  ~ mountpoint /run/media/veracrypt1
/run/media/veracrypt1 is a mountpoint
➜  ~ cat /run/media/veracrypt1/secret.txt
secret  message for lab6
➜  ~ veracrypt -d
➜  ~ mountpoint /run/media/veracrypt1
mountpoint: /run/media/veracrypt1: No such file or directory
➜  ~ grep -aF "secret  message for lab6" ~/my-coding/school/itmo/senior/infosec/infosec-cryptography/lab6.hc
[1]  + 202311 done       echo
➜  ~ echo $?
1
```

Строка `grep -aF` показывает, что содержимое не хранится в открытом виде в .hc файле.

== Генерация пары OpenPGP-ключей

Для выполнения части с асимметричным шифрованием была использована утилита GnuPG (`gpg`), реализующая стандарт OpenPGP. Пара ключей была создана локально без использования онлайн-генератора.

Был выбран алгоритм RSA с размером ключа 4096 бит. Для идентификатора пользователя были указаны имя `theodor` и адрес `theodor@example.com`.

Ниже приведён полный вывод терминала после подтверждения идентификатора пользователя и завершения генерации ключей.

```bash
Real name: theodor
Email address: theodor@example.com
Comment:
You selected this USER-ID:
    "theodor <theodor@example.com>"

Change (N)ame, (C)omment, (E)mail or (O)kay/(Q)uit? O
We need to generate a lot of random bytes. It is a good idea to perform
some other action (type on the keyboard, move the mouse, utilize the
disks) during the prime generation; this gives the random number
generator a better chance to gain enough entropy.
We need to generate a lot of random bytes. It is a good idea to perform
some other action (type on the keyboard, move the mouse, utilize the
disks) during the prime generation; this gives the random number
generator a better chance to gain enough entropy.
gpg: directory '/home/theodor/.gnupg/openpgp-revocs.d' created
gpg: revocation certificate stored as '/home/theodor/.gnupg/openpgp-revocs.d/87135DD5F77A82AB8BF1314E5126EB27BAA028DC.rev'
public and secret key created and signed.

pub   rsa4096 2026-10-07 [SC]
      87135DD5F77A82AB8BF1314E5126EB27BAA028DC
uid                      theodor <theodor@example.com>
sub   rsa4096 2026-10-07 [E]
```

Основной открытый ключ имеет отпечаток:

```text
87135DD5F77A82AB8BF1314E5126EB27BAA028DC
```

У основного ключа указаны возможности `[SC]`: подпись и сертификация. Для шифрования создан RSA-подключ с назначением `[E]`.

== Экспорт открытого ключа

Открытый ключ был экспортирован в ASCII-armored файл `public_key.asc` командой:

```bash
gpg --armor --export theodor@example.com > public_key.asc
```

После экспорта было проверено наличие файла в рабочем каталоге.

```bash
➜  infosec-cryptography gpg --armor --export theodor@example.com > public_key.asc
➜  infosec-cryptography l
total 101M
drwxr-xr-x  2 theodor theodor 4.0K Oct  8 01:44 .
drwxr-xr-x 19 theodor theodor 4.0K Oct  7 21:07 ..
-rw-------  1 theodor theodor 100M Oct  8 00:09 lab6.hc
-rw-r--r--  1 theodor theodor 3.1K Oct  8 01:44 public_key.asc
-rw-r--r--  1 theodor theodor 180K Oct  7 21:08 task.pdf
```

Затем содержимое экспортированного файла было проверено средствами GnuPG.

```bash
➜  infosec-cryptography gpg --show-keys public_key.asc
pub   rsa4096 2026-10-07 [SC]
      87135DD5F77A82AB8BF1314E5126EB27BAA028DC
uid                      theodor <theodor@example.com>
sub   rsa4096 2026-10-07 [E]
```

Отпечаток ключа в экспортированном файле совпал с отпечатком созданной пары, поэтому экспорт выполнен корректно.

== Шифрование и расшифрование сообщения

В качестве дополнительной части работы было выполнено шифрование текстового сообщения собственным открытым ключом и последующее расшифрование соответствующим закрытым ключом.

Сначала был создан файл `message.txt`, после чего команда `gpg --encrypt` сформировала зашифрованный файл `message.txt.gpg`.

Полный вывод терминала приведён ниже.

```bash
➜  infosec-cryptography gpg --show-keys public_key.asc
pub   rsa4096 2026-10-07 [SC]
      87135DD5F77A82AB8BF1314E5126EB27BAA028DC
uid                      theodor <theodor@example.com>
sub   rsa4096 2026-10-07 [E]

➜  infosec-cryptography echo "Hello, this is my encrypted message for lab 6" > message.txt
➜  infosec-cryptography gpg --encrypt --recipient theodor@example.com message.txt
➜  infosec-cryptography ls
lab6.hc  message.txt  message.txt.gpg  public_key.asc  task.pdf
➜  infosec-cryptography cat message.txt.gpg

�$�q{��*A����l{
               �
��D�=HZ��<�����٭ӣ=�$r6��
                        ��Yf����?�+����cV�EVˋO׳�n�{mR�bf��������/޳X`�tT:c�8/U�e<��)��b%?��r�^!m�Y�+;c�FY4ҕ��&?�9q1��ԘV
                                             D$�C��
�%~�;�u{ŵ�l%g�t6�94T|�i5�?7T��0�r�z�"]2���S��hx�_ȭ�r�&��3m���C6���yz(�lCn�F_5���;.&����>���	}f��LWa9u�?���֒ߍ�45���*d�����V�$WC8��p1�A0!ܨ��v��:�Nk[�6Z�OQ]v��x�G5Q-��nˁ�n�Q.5v�DCO
                                    ��x�'�?m�&0t��Lo{R��6�ڂx{����� �DrG�Җ'm{#r>�
       �7�<ߡ4-��Zݪ��׽%                                                   ➜  infosec-cryptography gpg --decrypt message.txt.gpg
gpg: encrypted with rsa4096 key, ID E224B2718C7BD20E, created 2026-10-07
      "theodor <theodor@example.com>"
Hello, this is my encrypted message for lab 6
```

Команда `cat` была применена к бинарному файлу `message.txt.gpg`, поэтому терминал вывел нечитаемую последовательность байтов. После выполнения `gpg --decrypt` исходное сообщение было восстановлено без изменений.

= Сравнение симметричного и асимметричного шифрования

При симметричном шифровании один секретный ключ используется как для шифрования, так и для расшифрования данных. В данной работе этот подход был продемонстрирован на VeraCrypt: доступ к содержимому контейнера определяется знанием пароля, на основе которого VeraCrypt получает необходимые криптографические ключи.

При асимметричном шифровании используются два связанных ключа. Открытый ключ разрешено передавать другим пользователям; с его помощью шифруется сообщение для владельца ключевой пары. Расшифрование выполняется соответствующим закрытым ключом, который не должен передаваться другим лицам. В работе данный принцип был продемонстрирован средствами GnuPG.

= Ответы на контрольные вопросы

== В чём основное различие между симметричным и асимметричным шифрованием?

В симметричном шифровании для шифрования и расшифрования используется один общий секретный ключ. В асимметричном шифровании используется пара ключей: открытый и закрытый. Операции, выполняемые одним ключом пары в предусмотренной схеме, проверяются или обращаются с помощью другого ключа пары.

== Как решается проблема безопасной передачи ключа в асимметричной криптографии?

Закрытый ключ не передаётся. Получатель распространяет только открытый ключ. Отправитель использует этот открытый ключ для шифрования данных, после чего расшифрование возможно только при наличии соответствующего закрытого ключа. Поэтому для передачи секрета не требуется предварительно передавать получателю общий секретный ключ по защищённому каналу.

При этом остаётся отдельная задача проверки подлинности открытого ключа: необходимо убедиться, что полученный ключ действительно принадлежит предполагаемому владельцу. Для этого могут использоваться сверка отпечатка ключа, доверенный канал или инфраструктура сертификатов.

== Для каких реальных задач применяется шифрование?

Примеры применения:

1. Защита данных на дисках и в файловых контейнерах, например с помощью VeraCrypt.
2. Защита сетевого обмена между клиентом и сервером, например при использовании TLS в HTTPS.
3. Шифрование электронной почты и файлов для конкретного получателя с помощью OpenPGP.

= Вывод

В ходе работы был создан и использован зашифрованный контейнер VeraCrypt с файловой системой ext4. В контейнер были помещены тестовые файлы. Затем была создана RSA-пара OpenPGP-ключей размером 4096 бит, открытый ключ был экспортирован в файл `public_key.asc`. Дополнительно было выполнено шифрование текстового сообщения открытым ключом и его последующее расшифрование закрытым ключом.

Требования задания выполнены: получен скриншот VeraCrypt со смонтированным томом, сформирован файл экспортированного открытого PGP-ключа и приведено краткое описание различий между симметричным и асимметричным шифрованием.
