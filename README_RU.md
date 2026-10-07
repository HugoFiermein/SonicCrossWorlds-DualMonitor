# CrossWorldsFix

[**English**](README.md) | [**Русский**](README_RU.md)

**CrossWorldsFix** — модификация (ASI-плагин) для игры **Sonic Racing: CrossWorlds** (Unreal Engine 5), добавляющая полноценную поддержку ультрашироких экранов (21:9, 32:9) и настоящий **сплит-скрин на 2 физических монитора** (AMD Eyefinity / NVIDIA Surround).

---

## Возможности

### 🎮 Сплит-скрин на 2 монитора (32:9 / 3840x1080 и выше)
- **Отдельный экран для каждого игрока**: весь интерфейс (HUD) и камера Игрока 1 отображаются на 1-м мониторе (слева), а Игрока 2 — на 2-м мониторе (справа).
- **Скрытие центральной полосы**: убирает внутриигровую черную вертикальную разделительную полосу, так как экраны уже физически разделены рамками мониторов.
- **Адаптация 2P меню под два экрана**: экраны выбора персонажей и машин (`WBP_Ready_M2`) разнесены по мониторам — Игрок 1 выбирает гонщика на левом мониторе, Игрок 2 на правом, меню больше не режется рамкой мониторов по центру.
- **Исправление прицела**: устранен баг, из-за которого при атаке вперед отображалась только правая половина прицела (теперь прицел отображается полностью и корректно позиционируется).

### 🖥️ Поддержка Ultrawide
- Устранение черных полос по бокам (pillarboxing) на 21:9, 32:9 и других соотношениях сторон.
- Корректная проекция маркеров позиций гонщиков и элементов HUD в 3D-мире.
- Опциональное растягивание одиночного HUD на всю ширину экрана (`Span Racing HUD`).

---

## Быстрая установка

Процесс максимально простой и выполняется в **1 действие**:

1. Скачайте архив релиза (`CrossWorldsFix-DualMonitor.zip`).
2. Скопируйте 3 файла из архива в папку игры с исполняемым файлом:
   ```
   SonicRacingCrossWorlds\UNION\Binaries\Win64\
   ```
   Содержимое:
   - `CrossWorldsFix.asi`
   - `CrossWorldsFix.ini`
   - `winmm.dll` (Ultimate ASI Loader x64)

3. Запустите игру через Steam как обычно!

### Steam Deck / Linux (Proton)
Если игра запускается через Proton/Wine:
- В свойствах игры в Steam в **Параметры запуска** добавьте:
  ```bash
  WINEDLLOVERRIDES="winmm=n,b" %command%
  ```

---

## Удаление

Чтобы полностью удалить мод, просто удалите 3 файла из папки `SonicRacingCrossWorlds\UNION\Binaries\Win64\`:
- `CrossWorldsFix.asi`
- `CrossWorldsFix.ini`
- `winmm.dll`

---

## Настройка

Все параметры настраиваются в файле [**`CrossWorldsFix.ini`**](CrossWorldsFix.ini):

```ini
[Fix Aspect Ratio]
Enabled = true          ; Убирает черные полосы по краям (32:9 / Ultrawide)

[Fix HUD]
Enabled = true          ; Исправляет масштабирование HUD и 3D-маркеры

[Dual Monitor Splitscreen]
Enabled = true          ; Включает привязку Игрока 1 к левому монитору, Игрока 2 к правому
HideCenterDividerLine = true ; Скрывает черную полосу по центру
Mirror2PMenus = true    ; Разносит меню выбора на 2 экрана без наложения на рамку
```

---

## Благодарности
- Огромная благодарность **Lyall** за базовую реализацию Ultrawide фикса.
- Разделение на 2 монитора и исправление прицела — **Hugo Fiermein**.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
- [safetyhook](https://github.com/cursey/safetyhook)
- [spdlog](https://github.com/gabime/spdlog)
- [inipp](https://github.com/mcmtroffaes/inipp)
- [Dumper-7](https://github.com/Encryqed/Dumper-7)
