#pragma once

#include <QString>
#include <vector>

struct CliAction {
    enum Type { Open, Crop, Delete, Skip, Speed, Scale, Save, Quit };
    Type type;
    QString arg;
};

struct CliArgs {
    bool headless = false;
    QString logLevel = "info";
    std::vector<CliAction> actions;
};

CliArgs parseCli(int argc, char *argv[]);
