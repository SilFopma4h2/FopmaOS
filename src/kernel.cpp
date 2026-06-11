#include <stdint.h>

#include "include/consts.h"
#include "include/vectors.h"
#include "include/memorys.h"
#include "include/screens.h"
#include "include/io.h"

// Helper-functie voor commando's
bool is_command(const vector<char>& input_vec, const char* cmd_str)
{
    size_t i = 0;
    while (cmd_str[i] != '\0')
    {
        if (i >= input_vec.size() || input_vec[i] != cmd_str[i])
        {
            return false;
        }
        i++;
    }
    return i == input_vec.size();
}

extern "C" void kernel_main(multiboot_info *mbi)
{
    // 1. Scherm leegmaken
    cls();

    // 2. Schone, VGA-proof opstartbanner
    print_string(
        "  ______                                ____   _____ \n"
        " |  ____|                              / __ \\ / ____|\n"
        " | |__ ___  _ __  _ __ ___   __ _     | |  | | (___  \n"
        " |  __/ _ \\| '_ \\| '_ ` _ \\ / _` |    | |  | |\\___ \\ \n"
        " | | | (_) | |_) | | | | | | (_| |    | |__| |____) |\n"
        " |_|  \\___/| .__/|_| |_| |_|\\__,_|     \\____/|_____/ \n"
        "           | |                                       \n"
        "           |_|                                       \n\n",
        VGA_COLOR_LIGHT_CYAN
    );

    // 3. Systeeminformatie
    print_string("Free Memory: ", VGA_COLOR_LIGHT_GREY);
    print_int(mbi->mem_lower + mbi->mem_upper);
    print_string("\n\n");

    // 4. Shell-loop
    while (true)
    {
        vector<char> i;
        print_string("- ", VGA_COLOR_LIGHT_BLUE);
        input(i);
        print_char('\n');

        if (is_command(i, "help"))
        {
            print_string(
                "FopmaOS - Beschikbare commando's:\n"
                "  help    - Toon dit menu\n"
                "  cls     - Maak het scherm leeg\n"
                "  ping    - Test of de kernel leeft\n",
                VGA_COLOR_LIGHT_GREEN
            );
        }
        else if (is_command(i, "clear"))
        {
            cls();
        }
        else if (is_command(i, "ping"))
        {
            print_string("Pong!\n", VGA_COLOR_LIGHT_GREEN);
        }
        else if (i.size() > 0)
        {
            print_string("Onbekend commando.\n", VGA_COLOR_LIGHT_RED);
        }
    }
}