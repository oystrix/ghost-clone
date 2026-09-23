# Сборка, публикация и отладка Ghost Clone

## 1. Подготовка

```bash
# Установка Geode CLI (если ещё не установлен)
pip install geode-cli   # либо через winget/brew — см. docs.geode-sdk.org

geode --version
geode sdk install       # скачает Geode SDK и попросит указать путь к GD
```

## 2. Сборка на Windows

```powershell
cd ghost-clone
geode build --platform win
```

Что нужно заранее:
- Visual Studio 2022 (с компонентом "Desktop development with C++")
- CMake ≥ 3.21 (обычно ставится вместе с VS)

Собранный `.geode` файл появится в `build/Release/GhostClone.geode` —
просто перетащите его в открытую GD (или в папку `geode/mods`).

## 3. Сборка на Android

Проще всего — через GitHub Actions (см. `.github/workflows/build.yml`):
1. Запушьте проект в свой репозиторий на GitHub.
2. Actions соберёт `Android32` и `Android64` автоматически.
3. Скачайте артефакт `ghost-clone` — внутри будет объединённый `.geode`
   (благодаря `combine: true` в workflow), который ставится и на Win, и на Android.

Локально Android собирается сложнее (нужен Android NDK r25c + CMake toolchain),
поэтому для мобильной сборки рекомендуется CI.

## 4. Сборка на iOS / macOS

Только на macOS с установленным Xcode:

```bash
geode build --platform mac   # для macOS
geode build --platform ios   # для iOS (нужен codesign / jailbreak-тулчейн)
```

Как и с Android, для iOS удобнее использовать GitHub Actions — раннер
`macos-latest` уже содержит всё необходимое.

## 5. Публикация в Geode Index через CLI

```bash
# Логин (откроет браузер для авторизации через GitHub)
geode login

# Проверка, что mod.json и файлы корректны
geode index validate .

# Публикация новой версии
geode index publish .
```

После публикации мод попадёт на модерацию — статус можно посмотреть на
https://geode-sdk.org/mods или командой `geode index status oystrix.ghost-clone`.

Чек-лист перед публикацией:
- [ ] Указан `repository` в `mod.json` со ссылкой на реальный GitHub-репозиторий
- [ ] Есть иконка мода `logo.png` (64×64) в корне проекта
- [ ] `about.md` заполнен и не содержит "TODO"
- [ ] Версия в `mod.json` увеличена относительно предыдущего релиза

## 6. Частые ошибки компиляции и их решения

| Ошибка | Причина | Решение |
|---|---|---|
| `fatal error: Geode/Geode.hpp: No such file` | Не установлен/не найден Geode SDK | `geode sdk install`, проверить переменную `GEODE_SDK` |
| `error: use of undeclared identifier 'PlayerObject'` | Не подключен нужный заголовок | Добавить `#include <Geode/modify/PlayerObject.hpp>` |
| `unresolved external symbol ... m_isDart` | Версия GD не совпадает с ожидаемой в bindings | Обновить Geode SDK (`geode sdk update`) до версии под GD 2.206 |
| `class ... does not name a type: Fields` | `Fields` объявлен после использования `m_fields` | `struct Fields { ... };` должен идти самым первым внутри `$modify` |
| Линковка падает на Android с `undefined reference` | Смешаны ABI (armv7 vs arm64) | Собирать `Android32` и `Android64` отдельными джобами (как в workflow) |
| Мод не появляется в списке после установки `.geode` | Файл собран не под ту версию GD | Проверить, что `gd` в `mod.json` соответствует установленной версии игры |

## 7. Отладка

- **Логи**: `geode::log::info/warn/error(...)` — смотрите их в GD DevTools
  (`Ctrl+Shift+D` на ПК, либо через `adb logcat` на Android с фильтром `Geode`).
- **DevTools GD** (встроены в Geode): открываются через меню Geode прямо в игре —
  позволяют инспектировать дерево нод, в том числе созданный `PlayerObject` призрака
  (ищите ноду с ID `ghost-clone-player`_spr).
- **Проверка сохранённых записей**: файлы лежат в
  `<GeodeSaveDir>/oystrix.ghost-clone/ghosts/ghost_<levelID>.json` — их можно
  открыть текстовым редактором и убедиться, что кадры пишутся.
- Если призрак "дёргается" — проверьте `fps` в JSON записи: он должен совпадать
  с частотой, на которой писались кадры (см. `GhostRecording::fps`).
