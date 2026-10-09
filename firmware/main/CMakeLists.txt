idf_component_register(SRCS "main.c" "log.c" "settings.c"
                            "ir/ir.cpp" "ir/ir_receiver_task.c"
                            "sensors/sensors.c"
                            "network/wifi.c" "network/web_auth.c" "network/web_server.c"
                            "bot/telegram.c" "bot/bot_commands.c" "bot/bot.c"
                            "automation/automation_rules.c" "automation/automation.c"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES "network/index.html")
