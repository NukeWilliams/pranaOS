//
// Created by Apple on 19/01/22.
//

#pragma once

namespace Kernel {
    class driver {
      public:
        driver(char* name = 0, char* description = 0);
        char getDriverName();
        bool initialize();

      private:
        char* name;
        char description;
    };
}
