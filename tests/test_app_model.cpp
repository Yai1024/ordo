// M1.5：界面模型的表单解析与展示映射（ADR-0002）。

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "ordo/ui/app_model.h"
#include "test_framework.h"

namespace {

const ordo::TimePoint kNow{std::chrono::hours{24 * 365}};

class FakeRepo final : public ordo::ITaskRepository {
public:
    std::vector<ordo::Task> tasks;

    ordo::Result<std::vector<ordo::Task>> load_all() const override {
        return ordo::Result<std::vector<ordo::Task>>::ok(tasks);
    }

    ordo::Status save_all(const std::vector<ordo::Task>& value) override {
        tasks = value;
        return ordo::Status::success();
    }

    ordo::Result<ordo::Task> find(ordo::TaskId id) const override {
        for (const ordo::Task& task : tasks) {
            if (task.id == id) {
                return ordo::Result<ordo::Task>::ok(task);
            }
        }
        return ordo::Result<ordo::Task>::fail(ordo::ErrorCode::NotFound, "找不到任务");
    }

    ordo::Status upsert(const ordo::Task& task) override {
        for (ordo::Task& existing : tasks) {
            if (existing.id == task.id) {
                existing = task;
                return ordo::Status::success();
            }
        }
        tasks.push_back(task);
        return ordo::Status::success();
    }

    ordo::Status erase(ordo::TaskId id) override {
        for (auto it = tasks.begin(); it != tasks.end(); ++it) {
            if (it->id == id) {
                tasks.erase(it);
                return ordo::Status::success();
            }
        }
        return ordo::Status::fail(ordo::ErrorCode::NotFound, "找不到任务");
    }
};

ordo::AppModel make_model(FakeRepo& repo) {
    return ordo::AppModel(repo, [] { return kNow; });
}

ordo::TaskForm make_form(const std::string& title = "高数作业") {
    ordo::TaskForm form;
    form.title = title;
    form.course = "高等数学";
    form.importance = "4";
    form.estimate = "90";
    form.deadline = "2026-12-01 20:00";
    return form;
}

ordo::TaskForm make_activity_form(const std::string& title = "社团例会") {
    ordo::TaskForm form;
    form.title = title;
    form.kind = "activity";
    form.importance = "4";
    form.estimate = "60";
    form.start = "2026-12-01 19:00";
    return form;
}

}  // namespace

ORDO_TEST(app_model_create_parses_form) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    const auto created = model.create(make_form());
    ORDO_CHECK(created.has_value());
    ORDO_CHECK_EQ(created.value(), ordo::TaskId{1});
    ORDO_CHECK_EQ(repo.tasks.size(), std::size_t{1});
    ORDO_CHECK_EQ(repo.tasks[0].title, std::string{"高数作业"});
    ORDO_CHECK_EQ(repo.tasks[0].importance, 4);
    ORDO_CHECK_EQ(repo.tasks[0].estimate, ordo::Minutes{90});
    ORDO_CHECK(repo.tasks[0].state == ordo::TaskState::Todo);
    ORDO_CHECK_EQ(ordo::format_datetime(repo.tasks[0].deadline),
                  std::string{"2026-12-01 20:00"});
}

ORDO_TEST(app_model_create_rejects_bad_input) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    ordo::TaskForm empty_title = make_form("   ");
    const auto empty = model.create(empty_title);
    ORDO_CHECK(!empty.has_value());
    ORDO_CHECK(empty.status().code == ordo::ErrorCode::EmptyInput);

    ordo::TaskForm bad_deadline = make_form();
    bad_deadline.deadline = "2026/12/01";
    const auto deadline = model.create(bad_deadline);
    ORDO_CHECK(!deadline.has_value());
    ORDO_CHECK(deadline.status().code == ordo::ErrorCode::InvalidFormat);

    ordo::TaskForm bad_importance = make_form();
    bad_importance.importance = "high";
    const auto importance = model.create(bad_importance);
    ORDO_CHECK(!importance.has_value());
    ORDO_CHECK(importance.status().code == ordo::ErrorCode::InvalidFormat);

    ordo::TaskForm out_of_range = make_form();
    out_of_range.importance = "9";
    const auto range = model.create(out_of_range);
    ORDO_CHECK(!range.has_value());
    ORDO_CHECK(range.status().code == ordo::ErrorCode::InvalidValue);

    ORDO_CHECK(repo.tasks.empty());  // 失败不落盘
}

ORDO_TEST(app_model_form_for_round_trips) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    const auto created = model.create(make_form("读书,第3章"));
    ORDO_CHECK(created.has_value());

    const auto form = model.form_for(created.value());
    ORDO_CHECK(form.has_value());
    ORDO_CHECK_EQ(form.value().title, std::string{"读书,第3章"});
    ORDO_CHECK_EQ(form.value().course, std::string{"高等数学"});
    ORDO_CHECK_EQ(form.value().importance, std::string{"4"});
    ORDO_CHECK_EQ(form.value().estimate, std::string{"90"});
    ORDO_CHECK_EQ(form.value().deadline, std::string{"2026-12-01 20:00"});
    ORDO_CHECK(form.value().state.has_value());
    ORDO_CHECK(*form.value().state == ordo::TaskState::Todo);
}

ORDO_TEST(app_model_create_activity_uses_start_and_computes_deadline) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    const auto created = model.create(make_activity_form());
    ORDO_CHECK(created.has_value());
    ORDO_CHECK_EQ(repo.tasks.size(), std::size_t{1});
    ORDO_CHECK(repo.tasks[0].kind == ordo::TaskKind::Activity);
    ORDO_CHECK(repo.tasks[0].fixed_start.has_value());
    ORDO_CHECK_EQ(ordo::format_datetime(*repo.tasks[0].fixed_start),
                  std::string{"2026-12-01 19:00"});
    ORDO_CHECK_EQ(ordo::format_datetime(repo.tasks[0].deadline),
                  std::string{"2026-12-01 20:00"});
}

ORDO_TEST(app_model_rejects_activity_without_start) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    ordo::TaskForm form = make_activity_form();
    form.start.clear();
    const auto result = model.create(form);

    ORDO_CHECK(!result.has_value());
    ORDO_CHECK(result.status().code == ordo::ErrorCode::InvalidFormat);
    ORDO_CHECK(repo.tasks.empty());
}

ORDO_TEST(app_model_form_for_round_trips_activity) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    const auto created = model.create(make_activity_form());
    ORDO_CHECK(created.has_value());

    const auto form = model.form_for(created.value());
    ORDO_CHECK(form.has_value());
    ORDO_CHECK_EQ(form.value().kind, std::string{"activity"});
    ORDO_CHECK_EQ(form.value().start, std::string{"2026-12-01 19:00"});
    ORDO_CHECK_EQ(form.value().deadline, std::string{"2026-12-01 20:00"});
}

ORDO_TEST(app_model_update_preserves_review_fields) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    const auto created = model.create(make_form());
    ORDO_CHECK(created.has_value());

    repo.tasks[0].review_stage = 3;

    ordo::TaskForm form = make_form("新标题");
    form.importance = "5";
    const ordo::Status updated = model.update(created.value(), form);
    ORDO_CHECK(updated.ok());

    ORDO_CHECK_EQ(repo.tasks[0].title, std::string{"新标题"});
    ORDO_CHECK_EQ(repo.tasks[0].importance, 5);
    ORDO_CHECK_EQ(repo.tasks[0].review_stage, 3);
}

ORDO_TEST(app_model_set_state_keeps_other_fields) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    const auto created = model.create(make_form("保持不变"));
    ORDO_CHECK(created.has_value());

    const ordo::Status marked = model.set_state(created.value(), ordo::TaskState::Doing);
    ORDO_CHECK(marked.ok());
    ORDO_CHECK(repo.tasks[0].state == ordo::TaskState::Doing);
    ORDO_CHECK_EQ(repo.tasks[0].title, std::string{"保持不变"});
}

ORDO_TEST(app_model_list_filters_and_maps_rows) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    const auto first = model.create(make_form("第一件"));
    const auto second = model.create(make_form("第二件"));
    ORDO_CHECK(first.has_value());
    ORDO_CHECK(second.has_value());
    ORDO_CHECK(model.set_state(second.value(), ordo::TaskState::Done).ok());

    const ordo::ListQuery open_query;
    const auto open_rows = model.list(open_query);
    ORDO_CHECK(open_rows.has_value());
    ORDO_CHECK_EQ(open_rows.value().size(), std::size_t{1});
    ORDO_CHECK_EQ(open_rows.value()[0].title, std::string{"第一件"});
    ORDO_CHECK_EQ(open_rows.value()[0].deadline_text, std::string{"2026-12-01 20:00"});

    ordo::ListQuery all_query;
    all_query.open_only = false;
    ORDO_CHECK_EQ(model.list(all_query).value().size(), std::size_t{2});

    ordo::ListQuery done_query;
    done_query.state = ordo::TaskState::Done;
    done_query.open_only = true;  // 指定状态时自动忽略 open_only
    const auto done_rows = model.list(done_query);
    ORDO_CHECK_EQ(done_rows.value().size(), std::size_t{1});
    ORDO_CHECK(done_rows.value()[0].state == ordo::TaskState::Done);
}

ORDO_TEST(app_model_list_ranks_by_score) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    ordo::TaskForm relaxed = make_form("宽松任务");
    relaxed.importance = "1";
    relaxed.estimate = "30";
    relaxed.deadline = "2026-12-30 20:00";
    ORDO_CHECK(model.create(relaxed).has_value());

    ordo::TaskForm urgent = make_form("紧急任务");
    urgent.importance = "5";
    urgent.estimate = "30";
    urgent.deadline = "2026-11-01 20:00";
    ORDO_CHECK(model.create(urgent).has_value());

    ordo::ListQuery query;
    query.rank = ordo::RankMode::Score;
    const auto rows = model.list(query);
    ORDO_CHECK(rows.has_value());
    ORDO_CHECK_EQ(rows.value()[0].title, std::string{"紧急任务"});
}

ORDO_TEST(app_model_plan_today_returns_formatted_rows) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);
    ORDO_CHECK(model.create(make_form("作业")).has_value());

    const auto plan = model.plan_today();
    ORDO_CHECK(plan.has_value());
    ORDO_CHECK_EQ(plan.value().total_free, ordo::Minutes{270});
    ORDO_CHECK_EQ(plan.value().total_assigned, ordo::Minutes{90});
    ORDO_CHECK_EQ(plan.value().rows.size(), std::size_t{1});
    ORDO_CHECK_EQ(plan.value().rows[0].title, std::string{"作业"});
    ORDO_CHECK_EQ(plan.value().rows[0].kind_label, std::string{"其他"});
    ORDO_CHECK_EQ(plan.value().rows[0].time_text, std::string{"18:30-20:00"});
}

ORDO_TEST(app_model_plan_today_reports_warnings) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    ordo::TaskForm form = make_form("大任务");
    form.estimate = "1000";
    ORDO_CHECK(model.create(form).has_value());

    const auto plan = model.plan_today();
    ORDO_CHECK(plan.has_value());
    ORDO_CHECK(!plan.value().warnings.empty());
    ORDO_CHECK(plan.value().warnings[0].find("#1") != std::string::npos);
}

ORDO_TEST(app_model_stats_reflects_repository) {
    FakeRepo repo;
    ordo::AppModel model = make_model(repo);

    ORDO_CHECK(model.create(make_form("未完成")).has_value());
    const auto done = model.create(make_form("已完成"));
    ORDO_CHECK(done.has_value());
    ORDO_CHECK(model.set_state(done.value(), ordo::TaskState::Done).ok());

    const auto stats = model.stats();
    ORDO_CHECK(stats.has_value());
    ORDO_CHECK_EQ(stats.value().total, std::size_t{2});
    ORDO_CHECK_EQ(stats.value().done, std::size_t{1});
    ORDO_CHECK_EQ(stats.value().open_estimate, ordo::Minutes{90});
}
