#include "cli.h"
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QDebug>

CliArgs parseCli(int argc, char *argv[]) {
    CliArgs args;
    for (int i = 1; i < argc; i++) {
        QString a = argv[i];
        if (a == "--headless") {
            args.headless = true;
        } else if (a == "--log-level" && i + 1 < argc) {
            args.logLevel = argv[++i];
        } else if (a == "--open" && i + 1 < argc) {
            args.actions.push_back({CliAction::Open, argv[++i]});
        } else if (a == "--crop" && i + 1 < argc) {
            args.actions.push_back({CliAction::Crop, argv[++i]});
        } else if (a == "--delete" && i + 1 < argc) {
            args.actions.push_back({CliAction::Delete, argv[++i]});
        } else if (a == "--skip" && i + 1 < argc) {
            args.actions.push_back({CliAction::Skip, argv[++i]});
        } else if (a == "--speed" && i + 1 < argc) {
            args.actions.push_back({CliAction::Speed, argv[++i]});
        } else if (a == "--scale" && i + 1 < argc) {
            args.actions.push_back({CliAction::Scale, argv[++i]});
        } else if (a == "--save" && i + 1 < argc) {
            args.actions.push_back({CliAction::Save, argv[++i]});
        } else if (a == "--help" || a == "-h") {
            qInfo() << R"(GIF Editor CLI
  --headless          不显示 GUI，处理后退出
  --open file.gif     打开文件
  --crop start-end    裁剪至范围
  --delete start-end  删除范围
  --skip N            每 N 帧保留 1 帧
  --speed N           倍速 (1.0=原速)
  --scale N           缩放 (1.0=原始)
  --save out.gif      保存到文件
  --log-level level   日志级别 (debug/info/warn)

Examples:
  gif-editor --headless --open in.gif --crop 10-50 --save out.gif
  gif-editor --headless --open in.gif --skip 2 --scale 0.5 --speed 2.0 --save out.gif
)";
            args.actions.push_back({CliAction::Quit, ""});
        } else if (!a.startsWith("-")) {
            // positional = open
            args.actions.push_back({CliAction::Open, a});
        }
    }

    // 设置日志级别
    if (args.logLevel == "debug")
        QLoggingCategory::setFilterRules("*.debug=true");
    else if (args.logLevel == "warn")
        QLoggingCategory::setFilterRules("*.debug=false\n*.info=false");

    return args;
}
