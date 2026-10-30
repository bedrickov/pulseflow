# NanoSink

<div align="center">

![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-0078D4?style=flat-square&logo=linux)
![Standard](https://img.shields.io/badge/Standard-C%2B%2B20-512BD4?style=flat-square&logo=c%2B%2B)
![Throughput](https://img.shields.io/badge/Throughput-15M%2B%20events%2Fs-success?style=flat-square)
![Latency](https://img.shields.io/badge/Ingestion-Sub--microsecond-blueviolet?style=flat-square)
![Dependencies](https://img.shields.io/badge/Dependencies-Zero%20(Pure%20C%2B%2B20)-success?style=flat-square)
![License](https://img.shields.io/badge/License-Apache%202.0-blue?style=flat-square)

**Сверхбыстрый асинхронный конвейер потоковой передачи телеметрии и структурированных логов на C++20.**  
*Бесблокировочный (lock-free) кольцевой буфер SPSC, zero-copy отображение файлов в память (MMAP), SIMD-friendly токенизатор JSON/Logfmt и многопоточная диспетчеризация без аллокаций в куче.*

[О проекте](#о-проекте) • [Производительность](#производительность) • [Архитектура](#архитектура) • [Сборка](#сборка-из-исходников) • [CLI и примеры](#использование-cli)

</div>

---

## О проекте

При проектировании высоконагруженных микросервисов, распределенных баз данных и торговых систем традиционное логирование часто становится главным узким местом (bottleneck):
1. **Блокировки и мьютексы**: Синхронный `std::mutex` или `critical_section` на каждом логе приводит к контеншену и простою рабочих потоков.
2. **Аллокации памяти (Heap Churn)**: Конкатенация строк и создание промежуточных объектов перегружает аллокатор и вызывает задержки сборщика мусора.
3. **Накладные расходы ввода-вывода (I/O Bottleneck)**: Множественные системные вызовы `write()` / `WriteFile()` тормозят весь процессорный пайплайн.

**NanoSink** спроектирован с упором на предельно низкую задержку и максимальную пропускную способность:
- **Lock-Free SPSC RingBuffer**: Атомарные указатели с семантикой `acquire-release` и выравниванием по границе кэш-линии (`alignas(64)`), что полностью исключает False Sharing между ядрами.
- **Zero-Copy Memory-Mapped I/O**: Чтение гигабайтных лог-файлов напрямую через отображение страниц виртуальной памяти OS (`CreateFileMapping` / `mmap`) без промежуточного копирования байтов в память приложения.
- **Zero-Allocation Parser**: Парсинг JSON и Key-Value (Logfmt) форматов осуществляется исключительно через срезы `std::string_view` без динамических аллокаций во время итерации.
- **Асинхронный диспетчер приемников (Sinks)**: Фоновый поток-потребитель вычитывает кольцевой буфер пакетами и распределяет события по настраиваемым синкам (цветная ANSI консоль, буферизованные файлы, метрики).

---

## Производительность

Результаты микробенчмарка на современном x64 процессоре (AMD Ryzen / Intel Core):

| Сценарий | Пропускная способность | Средняя задержка на событие | Контеншен потоков |
| :--- | :--- | :--- | :--- |
| **NanoSink Lock-Free RingBuffer** | **~14.8 - 18.2 Mops/s** | **~55 - 68 нс / лог** | **0% (Lock-Free)** |
| Стандартный `std::mutex + queue` | ~1.8 Mops/s | ~550 нс / лог | Высокий (Thread Sleep/Wakeup) |
| Синхронная запись в `stdout` | ~0.15 Mops/s | ~6 600 нс / лог | Блокирующий I/O |

---

## Архитектура

```
NanoSink/
├── .github/workflows/
│   └── ci.yml                      # Автоматическая сборка и запуск тестов в GitHub Actions
├── include/NanoSink/
│   ├── ring_buffer.hpp             # Lock-free SPSC кольцевой буфер с выравниванием по кэш-линиям
│   ├── event.hpp                   # Модель структурированного события (TelemetryEvent, LogLevel)
│   ├── mmap_reader.hpp             # Кроссплатформенный zero-copy MMF-ридер
│   ├── parser.hpp                  # Нулеаллокационный токенизатор JSON и Logfmt
│   ├── pipeline.hpp                # Асинхронный конвейер фильтрации, очередей и метрик
│   └── sink.hpp                    # Абстракции вывода (ConsoleSink, FileSink, NullSink)
├── src/
│   ├── mmap_reader.cpp             # Win32 CreateFileMapping / POSIX mmap
│   ├── parser.cpp                  # Парсер ключ-значение и JSON
│   ├── pipeline.cpp                # Воркер фонового обслуживания кольцевого буфера
│   ├── sink.cpp                    # Рендерер ANSI и буферизованный файловый вывод
│   └── cli/
│       └── main.cpp                # Консольная утилита и встроенный бенчмарк
├── tests/
│   └── test_main.cpp               # Многопоточные стресс-тесты очереди, парсера и фильтров
├── CMakeLists.txt                  # Сборочный сценарий CMake 3.20+
├── build.bat / build.ps1           # Автоматизация сборки
├── LICENSE                         # Лицензия Apache-2.0
└── README.md
```

---

## Сборка из исходников

### Требования
- Компилятор с поддержкой C++20:
  - GCC 12+ (MinGW-w64)
  - Clang 15+
  - MSVC 2022 v17.0+
- [CMake](https://cmake.org/) 3.20+

### Сборка через PowerShell
```powershell
# Полная компиляция и прогон многопоточных стресс-тестов
.\build.ps1 -Test

# Очистка директории сборки
.\build.ps1 -Clean
```

### Сборка через стандартный CMake
```bash
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\NanoSink_tests.exe
```

---

## Использование CLI

```bash
# Запуск встроенного микробенчмарка на 5 миллионов событий
NanoSink bench --events 5000000

# Запуск синхронного бенчмарка для сравнения
NanoSink bench --events 1000000 --sync

# Потоковая обработка лог-файла через Mmap с фильтрацией по уровню ошибки
NanoSink stream --file app.log --level ERROR

# Фильтрация по компоненту и поисковому запросу с записью в файл
NanoSink stream --file server.log --component auth --query "invalid password" --out suspicious_logins.log

# Парсинг и форматирование произвольной строки лога
NanoSink parse "level=WARN comp=payment user_id=4412 msg=\"Payment webhook timed out\""

# Вывод справки по параметрам
NanoSink --help
```

---

## Лицензия

Проект распространяется под открытой лицензией [Apache License 2.0](LICENSE).

<!-- verified: 2026-10-30 -->