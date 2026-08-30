#include <QApplication>
#include <QMainWindow>
#include "ui/ChatWidget.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle("AgenticAI - Local AI Studio");
    window.resize(1100, 850);

    ChatWidget *chatWidget = new ChatWidget(&window);
    window.setCentralWidget(chatWidget);

    window.show();

    return app.exec();
}
