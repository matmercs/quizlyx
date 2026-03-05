# Бизнес-логика сервера Quizlyx — краткая справка

## Модели и слои

**Домен** (`server/domain/`) — только данные и правила, без потоков и I/O.

- **types.hpp** — `SessionState` (Lobby → Running → Finished), `Role` (Host/Player), `AnswerType` (SingleChoice/MultipleChoice).
- **quiz.hpp / quiz.cpp** — `Quiz` (title, description, questions), `Question` (text, options, correct_indices, time_limit_ms). Валидация: `Validate(Question)`, `Validate(Quiz)`.
- **session.hpp / session.cpp** — `Session` (id, pin, quiz_code, host_id, state, players, current_question_index, question_deadline), `Player` (id, role, score, answered_current_question). Правила: `CanJoin`, `AddPlayer`, `CanStartGame`, `StartGame`, `CanSubmitAnswer`, `RecordAnswer`, `AdvanceToNextQuestion`, `RemovePlayer`.
- **answer.hpp** — `PlayerAnswer` (selected_indices, time_since_question_start_ms).

**События** (`server/events/game_events.hpp`) — типы для рассылки, без I/O: `QuestionStarted`, `TimerUpdate`, `QuestionTimeout`, `Leaderboard`/`LeaderboardEntry`, `PlayerJoined`, `PlayerLeft`, `GameFinished`. Общий тип: `GameEvent = std::variant<...>`.

**Порты (интерфейсы)** (`server/interfaces/`) — контракт для верхнего слоя.

- **IBroadcastSink** — `Broadcast(session_id, GameEvent)`; реализация потом шлёт по сети.
- **IQuizStorage** — `Create(Quiz) → code`, `Get(code) → optional<Quiz>`.
- **ITimeProvider** — `Now() → time_point`; для таймеров и тестов.
- **ICommandHandler** — приём команд: CreateQuiz, CreateSession, StartGame, NextQuestion, JoinAsPlayer, LeaveSession, SubmitAnswer. Реализация вызывает сервисы.

**Сервисы** (`server/services/`) — потокобезопасная логика, обращаются к домену и портам.

- **InMemoryQuizStorage** — реализация `IQuizStorage`, квизы в `unordered_map`, один `mutex`.
- **QuizRegistry** — обёртка над хранилищем: `Create` (валидация + сохранение), `Get`.
- **Scoring** — `CalculatePoints(question, answer)`: правильность + бонус за скорость (быстрее — больше очков).
- **SessionManager** — сессии по id и PIN; создание, join/leave, StartGame, NextQuestion, SubmitAnswer; вызов Scoring и `IBroadcastSink` при событиях. Один `mutex` на все операции; `Broadcast` вызывается после снятия блокировки.
- **SessionTimerService** — дедлайны по session_id; `SetDeadline`, `ClearDeadline`, `Tick()` возвращает список `TimerEvent` (TimerUpdate / QuestionTimeout). Свой `mutex`; `Tick()` вызывается из Timer Thread.

**Приложение** (`server/app/`) — **ServerCommandHandler**: реализация `ICommandHandler`, делегирует в `QuizRegistry` и `SessionManager`.

---

## Как работает таймер

**SessionTimerService** хранит по каждой сессии только дедлайн (момент окончания таймера) и время последней рассылки. О вопросах и сессиях не знает — только `session_id` и время.

- **Конструктор**: принимает `ITimeProvider` (источник «сейчас») и `update_interval` (интервал, с которым слать `TimerUpdate`, например 50–100 ms).
- **SetDeadline(session_id, deadline)** — запомнить дедлайн для сессии (вызывается при старте вопроса: сейчас в коде не вызывается; при появлении сети/оркестрации вызовет SessionManager при StartGame / NextQuestion).
- **ClearDeadline(session_id)** — убрать дедлайн (конец вопроса, конец игры или таймаут).
- **Tick()** — вызывается периодически из одного Timer Thread (раз в 50–100 ms). Под своим мьютексом для каждой сессии с дедлайном:
  - если **now ≥ deadline** → в результат добавляется событие **QuestionTimeout**, сессия удаляется из списка дедлайнов;
  - иначе, если с **последнего TimerUpdate** прошло не меньше `update_interval` → в результат добавляется **TimerUpdate(remaining_ms)** и обновляется время последней рассылки.
  Возвращает список **TimerEvent** (session_id + GameEvent). Кто вызвал Tick() (Timer Thread), тот для каждого элемента списка вызывает **Broadcast(session_id, event)** — рассылка по сети не входит в сервис.

**Итог**: таймер только считает дедлайны и выдаёт события; установку дедлайнов при старте вопроса и рассылку событий делает верхний слой (оркестрация + сеть).

**Timer Thread с точки зрения многопоточности.** Один выделенный поток: в цикле засыпает на 50–100 ms, вызывает `SessionTimerService::Tick()`, по возвращённому списку для каждого события вызывает `Broadcast(session_id, event)`. Таймер-сервис держит только свой мьютекс внутри `Tick()`; с SessionManager общих замков нет — `SetDeadline`/`ClearDeadline` из воркеров (при StartGame/NextQuestion) и `Tick()` из Timer Thread конкурируют только за мьютекс внутри SessionTimerService, взаимных блокировок с SessionManager не возникает. `Broadcast` будет вызываться и из воркеров (события от команд), и из Timer Thread (TimerUpdate/QuestionTimeout) — реализация `IBroadcastSink` должна быть потокобезопасной (очередь или свой замок на рассылку по сессии).

---

## Многопоточность (кратко)

- **Домен** — без мьютексов и потоков.
- **InMemoryQuizStorage** — один `mutex` на Create/Get.
- **SessionManager** — один `mutex` на сессии и все методы; событие формируется под замком, `Broadcast` — после.
- **SessionTimerService** — свой `mutex`; `Tick()` рассчитан на вызов из одного Timer Thread.
- **main** пока потоков не создаёт; Acceptor/Workers/Timer Thread — дальше, при появлении сети.

---

## Дальше: сервер с сетью

- Сетевой слой получит реализацию `ICommandHandler` (уже есть) и вызовет его из воркеров по приходу команд.
- Реализация `IBroadcastSink` будет рассылать события по сокетам всем участникам сессии (атомарно под своей блокировкой).
- Timer Thread раз в 50–100 ms будет вызывать `SessionTimerService::Tick()` и по списку событий вызывать `Broadcast`.
- `ITimeProvider` в проде — реальное время; в тестах — подмена.

---

## Тестовый сценарий (ServerBusinessLogicDemo.FullScenario)

Один сквозной тест: квиз из двух вопросов (один выбор + несколько вариантов), сессия, host + два игрока (alice, bob).

**Что происходит по шагам:**

1. Создание квиза → проверка кода.
2. Создание сессии → проверка session_id и PIN (6 цифр); состояние Lobby, один игрок (host), без дедлайна.
3. Join alice и bob по PIN → в лобби 3 участника.
4. Join по неверному PIN → ожидается отказ.
5. StartGame → состояние Running, вопрос 0, дедлайн установлен.
6. Ответы на вопрос 0: alice верно за 200 ms, bob верно за 2000 ms → проверка, что у alice очков больше (бонус за скорость).
7. Второй ответ alice на тот же вопрос → ожидается отказ.
8. NextQuestion → вопрос 1, флаги «ответил» сброшены.
9. Вопрос 1: alice неверно, bob верно → у alice очки не растут, у bob растут.
10. NextQuestion → вопросов больше нет → состояние Finished, текущий индекс = 2.
11. Итог: bob впереди по очкам.
12. Join по PIN в Running → отказ; SubmitAnswer после окончания игры → отказ.

**Как получить вывод состояний (как в старом main):**

- Тест **VerboseScenario** повторяет тот же сценарий и на каждом шаге выводит в консоль состояние сессии (state, question, has_deadline, список игроков с score и answered_current). Запуск:
  ```bash
  ./build/tests/QtBoostCMake_tests --gtest_filter="ServerBusinessLogicDemo.VerboseScenario"
  ```
  В консоли будет полный лог как в изначальном демо в main.
- Обычный **FullScenario** только проверяет инварианты (без вывода); при падении GTest покажет сообщения из `EXPECT_*`/`ASSERT_*`.
