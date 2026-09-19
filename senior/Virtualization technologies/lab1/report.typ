#set document(title: "Лабораторная работа № 1 — Установка гостевой ОС", author: "Молчанов Фёдор Денисович")
#set text(font: ("Times New Roman", "Liberation Serif"), size: 14pt, lang: "ru")
#set par(justify: true, first-line-indent: 1.25cm, leading: 0.5em)
#set heading(numbering: "1.1")
#set figure(numbering: "1")
#show heading.where(level: 1): it => {
  pagebreak(weak: true)
  align(center, text(size: 16pt, weight: "bold", it.body))
  v(10pt)
}
#show heading.where(level: 2): it => {
  v(8pt)
  align(center, text(size: 15pt, weight: "bold", it.body))
  v(6pt)
}
#show heading.where(level: 3): it => {
  v(6pt)
  text(size: 14pt, weight: "bold", it.body)
  v(4pt)
}
#show raw: set text(font: ("Liberation Mono", "DejaVu Sans Mono"), size: 10.5pt)
#show figure.caption: set text(size: 14pt)

// Титульный лист
#set page(paper: "a4", margin: (left: 3cm, right: 2cm, top: 1cm, bottom: 1cm), numbering: none)
#align(center)[
  Министерство науки и высшего образования Российской Федерации \
  федеральное государственное автономное образовательное учреждение высшего образования \
  *«Национальный исследовательский университет ИТМО»* \
  #v(16pt)
  Факультет программной инженерии и компьютерной техники
]
#v(1fr)
#align(center)[
  #text(size: 16pt, weight: "bold")[ЛАБОРАТОРНАЯ РАБОТА № 1] \
  #v(10pt)
  #text(size: 16pt, weight: "bold")[Установка гостевой ОС]
]
#v(1fr)
#align(right)[
  Группа: *P3413* \
  Выполнил: Молчанов Фёдор Денисович \
  #v(10pt)
  Проверил: Белозубов А.В. \
]
#v(1fr)
#align(center)[Санкт-Петербург, 2026]
#pagebreak()

// Основная часть
#set page(
  paper: "a4",
  margin: (left: 3cm, right: 1cm, top: 2cm, bottom: 2cm),
  numbering: "1",
  number-align: center + bottom,
)

#align(center, text(size: 16pt, weight: "bold")[ОГЛАВЛЕНИЕ])
#outline(title: none, depth: 3)
#pagebreak()

= ВВЕДЕНИЕ

Цель работы — получить практические навыки создания и настройки виртуальных машин в Oracle VirtualBox, установки гостевых операционных систем, настройки виртуальных сетей различных типов, использования снимков состояния, общих папок, буфера обмена и управления виртуальными машинами из командной строки.

Работа выполнялась на хостовой ОС Arch Linux. Использовался Oracle VirtualBox 7.2.16 и две гостевые ОС: Windows 10 22H2 x64 и Ubuntu 26.04.1 Desktop amd64. Для обеих виртуальных машин применялись минимальные параметры, указанные в задании: 1 виртуальный процессор, 2048 МБ оперативной памяти и виртуальный диск 20 ГБ. В ходе работы отдельные параметры изменялись в соответствии с последующими пунктами задания.

= ПОСТАНОВКА ЗАДАЧИ

В рамках лабораторной работы требовалось:

- установить и настроить VirtualBox и Extension Pack;
- создать и установить две гостевые ОС — Windows 10 и Linux;
- установить Guest Additions;
- исследовать режимы сети Internal Network, Host-Only, NAT и NAT Network;
- проверить статическую и динамическую адресацию, DHCP и сетевую связность;
- создать и восстановить снимки состояния виртуальной машины;
- настроить общую папку, двунаправленный буфер обмена и Drag-and-Drop;
- выполнить управление виртуальными машинами через `VBoxManage` и создать скрипты запуска.

= ПОДГОТОВКА ОКРУЖЕНИЯ И СОЗДАНИЕ ВИРТУАЛЬНЫХ МАШИН

== Проверка хостовой системы

Перед началом установки была проверена версия ядра, наличие заголовков ядра, VirtualBox, DKMS-модулей и аппаратной виртуализации AMD-V.

```text
$ uname -r
7.2.4-arch1-2

$ pacman -Q virtualbox virtualbox-host-dkms
virtualbox 7.2.16-1
virtualbox-host-dkms 7.2.16-1

$ pacman -Q linux linux-headers
linux 7.2.4.arch1-2
linux-headers 7.2.4.arch1-2

$ dkms status
vboxhost/7.2.16_OSE, 7.2.4-arch1-2, x86_64: installed

$ lscpu | grep Virtualization
Virtualization: AMD-V
```

Было установлено, что версии ядра и заголовков совпадают, DKMS-модуль VirtualBox загружен, а аппаратная виртуализация AMD-V включена.

== Создание Ubuntu и Windows VM

Для Ubuntu была создана виртуальная машина `WS_MolchanovFyodorDenisovich_ubuntu`, а для Windows — `WS_MolchanovFyodorDenisovich_win`. Использовались динамические виртуальные диски VDI объёмом 20 ГБ; физические разделы хостовой системы напрямую не подключались.

#figure(
  image("screenshots/used/01_ubuntu_vm_summary.png", width: 100%),
  caption: [Итоговые параметры виртуальной машины Ubuntu перед созданием],
)

#figure(
  image("screenshots/used/02_windows_vm_summary.png", width: 100%),
  caption: [Итоговые параметры виртуальной машины Windows перед созданием],
)

Для обеих машин звук был отключён. На первом сетевом этапе адаптеры были подключены к общей внутренней сети `intnet`.

== Установка гостевых ОС

Ubuntu устанавливалась в интерактивном режиме. На этапе разметки установщик видел виртуальный диск `VBOX HARDDISK sda`; был выбран вариант удаления содержимого виртуального диска и установки Ubuntu.

#figure(
  image("screenshots/used/03_ubuntu_install_disk.png", width: 100%),
  caption: [Выбор виртуального диска VBOX HARDDISK для установки Ubuntu],
)

Windows 10 была установлена в режиме чистой установки `Custom: Install Windows only`. После завершения установки ISO-образы были извлечены из виртуальных оптических приводов.

= УСТАНОВКА GUEST ADDITIONS

В Windows Guest Additions были установлены с виртуального диска `VirtualBox Guest Additions`, после чего система была перезагружена. Установка подтверждена командой на хосте:

```text
$ VBoxManage guestproperty get "WS_MolchanovFyodorDenisovich_win" /VirtualBox/GuestAdd/Version
Value: 7.2.16
```

#figure(
  image("screenshots/used/04_windows_guest_additions.png", width: 100%),
  caption: [Windows после установки Guest Additions и автоматического изменения разрешения],
)

В Ubuntu при запуске установщика Guest Additions первоначально отсутствовал пакет `bzip2`. Для установки зависимостей временно использовался режим NAT; были установлены `bzip2`, `build-essential`, `dkms` и заголовки текущего ядра. После выполнения `VBoxLinuxAdditions.run` были проверены модули ядра и пользовательские сервисы.

#figure(
  image("screenshots/used/05_ubuntu_guest_additions_status.png", width: 100%),
  caption: [Проверка модулей и пользовательских сервисов Guest Additions в Ubuntu],
)

Команда на хосте также показала версию `7.2.16`. Для Ubuntu был оставлен графический контроллер VBoxSVGA, поскольку с VMSVGA Ubuntu 26.04 выводила сообщение `vmwgfx ... unsupported hypervisor`. Guest Additions при этом успешно работали, однако автоматическое изменение разрешения в Wayland было ограничено.

= ИССЛЕДОВАНИЕ СЕТЕВЫХ РЕЖИМОВ

== Internal Network

Для режима Internal Network обе виртуальные машины были подключены к сети `intnet`. Сначала были назначены статические адреса:

- Windows — `192.168.99.1/24`;
- Ubuntu — `192.168.99.2/24`.

В Ubuntu адрес задавался с помощью `nmcli`, в Windows — через PowerShell. При первой проверке Windows успешно пинговала Ubuntu, а обратный ping блокировался Windows Defender Firewall.

#figure(
  image("screenshots/used/06_internal_static_config.png", width: 100%),
  caption: [Настройка статических IPv4-адресов Windows и Ubuntu],
)

#figure(
  image("screenshots/used/07_internal_ping_before_firewall.png", width: 100%),
  caption: [Асимметричная связность до разрешения входящих ICMP Echo Request в Windows],
)

Было добавлено правило Windows Firewall, разрешающее входящие ICMPv4 Echo Request. После этого ping работал в обоих направлениях, но внешний адрес `1.1.1.1` оставался недоступен, что соответствует изолированному характеру Internal Network.

#figure(
  image("screenshots/used/08_internal_ping_after_firewall.png", width: 100%),
  caption: [Двусторонний ping между VM и отсутствие доступа в Интернет в Internal Network],
)

После перевода интерфейсов в автоматический режим получения адресов DHCP-сервера в `intnet` не оказалось, поэтому гостевые ОС не получили нормальные IPv4-адреса. Это ожидаемый результат для внутренней сети без настроенного DHCP.

#figure(
  image("screenshots/used/09_dhcp_no_server.png", width: 100%),
  caption: [Автоматическое получение адресов при отсутствии DHCP-сервера во внутренней сети],
)

== Host-Only

На хосте был создан интерфейс `vboxnet0` с адресом `192.168.56.1/24`. Виртуальные машины были подключены к Host-Only Adapter, после чего встроенный DHCP VirtualBox выдал адреса `192.168.56.101` и `192.168.56.102`.

#figure(
  image("screenshots/used/10_hostonly_adapter.png", width: 100%),
  caption: [Подключение виртуальной машины к Host-Only Adapter vboxnet0],
)

Затем, в соответствии с заданием, был создан второй Host-Only интерфейс `vboxnet1` с адресом `192.168.99.1/24`. Для работы диапазона `192.168.99.0/24` в Arch Linux был добавлен `/etc/vbox/networks.conf`. DHCP-сервер `vboxnet1` был настроен на диапазон `192.168.99.10–192.168.99.77`.

#figure(
  image("screenshots/used/11_hostonly_dhcp_addresses.png", width: 100%),
  caption: [Получение адресов из DHCP-диапазона 192.168.99.10–77],
)

Проверка показала доступ между гостевыми ОС и доступ к адресу хоста `192.168.99.1`, но внешний адрес `1.1.1.1` был недоступен.

#figure(
  image("screenshots/used/12_hostonly_ping.png", width: 90%),
  caption: [Проверка Host-Only: связь с VM и хостом при отсутствии выхода в Интернет],
)

== NAT

Обе VM были переведены в обычный режим NAT. В результате каждая машина получила стандартный адрес VirtualBox NAT `10.0.2.15/24` и шлюз `10.0.2.2`. Одинаковые адреса не являются конфликтом, поскольку для каждой VM используется отдельный NAT-механизм.

#figure(
  image("screenshots/used/13_nat_addresses.png", width: 100%),
  caption: [Адреса гостевых ОС в режиме NAT],
)

Проверка `1.1.1.1`, `google.com` и `archive.ubuntu.com` показала наличие внешней связности и DNS.

#figure(
  image("screenshots/used/14_nat_internet.png", width: 100%),
  caption: [Доступ в Интернет из обеих гостевых ОС в режиме NAT],
)

== NAT Network

Была создана сеть `NatNetwork` с адресом `10.45.33.0/24`. При первом запуске Windows получила конфликтующий адрес `10.45.33.3`, который одновременно использовался служебным DNS NAT Network. Нижняя граница DHCP-пула была изменена на `10.45.33.4`, после чего Ubuntu получила `10.45.33.4`, а Windows — `10.45.33.5`.

#figure(
  image("screenshots/used/15_natnetwork_addresses.png", width: 100%),
  caption: [Разные DHCP-адреса Windows и Ubuntu в общей NAT Network],
)

В общей NAT Network виртуальные машины успешно пинговали друг друга и одновременно имели доступ во внешнюю сеть.

#figure(
  image("screenshots/used/16_natnetwork_connectivity.png", width: 100%),
  caption: [Связность между VM и доступ в Интернет в NatNetwork],
)

После этого была создана вторая сеть `NatNetwork1` с адресом `10.22.77.0/24`. Ubuntu была переведена в `NatNetwork1`, а Windows оставлена в исходной `NatNetwork`. В результате доступ между VM пропал, но выход в Интернет сохранился у обеих систем.

#figure(
  image("screenshots/used/17_natnetwork1_isolation.png", width: 100%),
  caption: [Изоляция разных NAT Network при сохранении доступа в Интернет],
)

= СНИМКИ СОСТОЯНИЯ

Для Windows был создан исходный снимок `Новая ОС Windows`.

#figure(
  image("screenshots/used/18_snapshot_new_windows.png", width: 85%),
  caption: [Создание исходного снимка Windows],
)

После установки Yandex Browser был создан снимок `ОС Windows+Yandex`.

#figure(
  image("screenshots/used/19_snapshot_yandex_tree.png", width: 85%),
  caption: [Дерево снимков после установки Yandex Browser],
)

Затем был восстановлен снимок `Новая ОС Windows`. Поиск Yandex Browser в списке приложений не дал результатов, что подтвердило откат состояния виртуального диска.

#figure(
  image("screenshots/used/20_snapshot_restore_yandex_absent.png", width: 100%),
  caption: [Отсутствие Yandex Browser после восстановления исходного снимка],
)

Вместо МойОфис в качестве второго тестового приложения использовался 7-Zip, поскольку он имеет существенно меньший установочный пакет и быстрее устанавливается на VM с одним процессором. После установки 7-Zip параметры VM были увеличены до 4096 МБ RAM и 2 CPU, затем был создан отдельный снимок состояния.

#figure(
  image("screenshots/used/21_7zip_installed.png", width: 100%),
  caption: [Наличие 7-Zip в состоянии ОС Windows+7zip],
)

#figure(
  grid(
    columns: (1fr, 1fr),
    gutter: 10pt,
    image("screenshots/used/22_snapshot_yandex_resources.png", width: 100%),
    image("screenshots/used/23_snapshot_7zip_resources.png", width: 100%),
  ),
  caption: [Различие аппаратной конфигурации снимков: 2048 МБ/1 CPU и 4096 МБ/2 CPU],
)

После восстановления `ОС Windows+Yandex` вернулись прежние ресурсы и установленный Yandex Browser, а 7-Zip исчез. Таким образом, снимки позволили восстановить как содержимое виртуального диска, так и конфигурацию VM.

= ОБЩАЯ ПАПКА, БУФЕР ОБМЕНА И DRAG-AND-DROP

На хосте была создана папка `/home/theodor/Public`. Она была добавлена в обе VM как постоянная Shared Folder с автоматическим подключением и полным доступом.

#figure(
  image("screenshots/used/26_shared_folder_settings.png", width: 100%),
  caption: [Настройка общей папки Public],
)

Для обеих VM были включены двунаправленный буфер обмена и Drag-and-Drop.

#figure(
  grid(
    columns: (1fr, 1fr),
    gutter: 10pt,
    image("screenshots/used/25_clipboard_bidirectional.png", width: 100%),
    image("screenshots/used/24_dragdrop_bidirectional.png", width: 100%),
  ),
  caption: [Двунаправленные Shared Clipboard и Drag-and-Drop],
)

В Windows общая папка подключилась как сетевой диск `Public (\\VBoxSvr) (Z:)`. В Ubuntu она была смонтирована в `/media/sf_Public`; для доступа обычного пользователя он был добавлен в группу `vboxsf`. Затем из Ubuntu был создан файл `from_ubuntu.txt`, который сразу стал доступен в Windows.

#figure(
  image("screenshots/used/27_shared_folder_result.png", width: 100%),
  caption: [Проверка общей папки: файл, созданный в Ubuntu, открыт в Windows],
)

= УПРАВЛЕНИЕ VIRTUALBOX ИЗ КОМАНДНОЙ СТРОКИ

Список зарегистрированных виртуальных машин был получен командой:

```text
$ VBoxManage list vms
"WS_MolchanovFyodorDenisovich_ubuntu" {c36d64f2-beb3-44cc-9263-a58a85fa6e6d}
"WS_MolchanovFyodorDenisovich_win" {fecad778-a872-4133-b91f-ab6ee518393a}
```

Для просмотра информации использовалась команда:

```text
$ VBoxManage showvminfo "WS_MolchanovFyodorDenisovich_ubuntu" | head -n 25
Name: WS_MolchanovFyodorDenisovich_ubuntu
Memory size: 2048MB
Number of CPUs: 1
...
```

Запуск VM из командной строки:

```text
$ VBoxManage startvm "WS_MolchanovFyodorDenisovich_ubuntu" --type gui
Waiting for VM "WS_MolchanovFyodorDenisovich_ubuntu" to power on...
VM "WS_MolchanovFyodorDenisovich_ubuntu" has been successfully started.
```

Для автоматизации были созданы два shell-скрипта:

```bash
#!/bin/bash
VBoxManage startvm "WS_MolchanovFyodorDenisovich_ubuntu" --type gui
```

```bash
#!/bin/bash
VBoxManage startvm "WS_MolchanovFyodorDenisovich_win" --type gui
```

Оба скрипта были сделаны исполняемыми и успешно проверены:

```text
$ ./start-ubuntu.sh
VM "WS_MolchanovFyodorDenisovich_ubuntu" has been successfully started.

$ ./start-windows.sh
VM "WS_MolchanovFyodorDenisovich_win" has been successfully started.
```

= ЗАКЛЮЧЕНИЕ

В ходе лабораторной работы были созданы и установлены две гостевые операционные системы в Oracle VirtualBox. Установлены Guest Additions и проверена интеграция гостевых ОС с хостом. Практически исследованы четыре основных режима виртуальной сети: Internal Network, Host-Only, NAT и NAT Network.

Экспериментально подтверждено, что Internal Network обеспечивает изолированную связь между VM; Host-Only добавляет связь с хостом, но не даёт прямого выхода во внешнюю сеть; обычный NAT предоставляет каждой VM отдельный доступ во внешнюю сеть; NAT Network одновременно обеспечивает взаимодействие гостевых машин и доступ в Интернет. Также была исследована изоляция двух разных NAT Network.

Снимки состояния позволили вернуть установленное программное обеспечение и аппаратные параметры VM к ранее зафиксированным значениям. Общая папка обеспечила обмен файлами между Arch Linux, Windows и Ubuntu. В завершение были отработаны основные команды `VBoxManage` и созданы скрипты запуска виртуальных машин.

= ЛИТЕРАТУРА

1. Методические указания к лабораторной работе № 1 «Установка гостевой ОС».
2. Требования к оформлению отчёта по лабораторным работам.
3. Oracle. *Oracle VirtualBox User Guide for Release 7.2*. https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/
4. ArchWiki. *VirtualBox*. https://wiki.archlinux.org/title/VirtualBox
