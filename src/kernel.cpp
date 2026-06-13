#include <stdint.h>

#include "include/consts.h"
#include "include/vectors.h"
#include "include/memorys.h"
#include "include/screens.h"
#include "include/io.h"
const char* version = "v0.0.1 beta";
// ============================================================================
// GLOBALE VARIABELEN & BUFFERS
// ============================================================================
// Deze buffer blijft in het RAM-geheugen bestaan, ook als je de editor sluit.
vector<char> globale_tekst_buffer;


// ============================================================================
// TOETSENBORD INPUT FUNCTIES
// ============================================================================

/**
 * Wacht op een toetsaanslag en vertaalt deze naar een ASCII-karakter.
 * Houdt ook rekening met de Shift-toetsen.
 */
char read_char()
{
    static bool left_shift = false;
    static bool right_shift = false;

    while (true)
    {
        uint8_t scancode = scankey();

        switch (scancode)
        {
            // Shift indrukken / loslaten
            case SHIFT_PRESSED_LEFT:    left_shift = true;   break;
            case SHIFT_RELEASED_LEFT:   left_shift = false;  break;
            case SHIFT_PRESSED_RIGHT:   right_shift = true;  break;
            case SHIFT_RELEASED_RIGHT:  right_shift = false; break;

            // Speciale toetsen
            case ENTER:     return '\n';
            case BACKSPACE: return '\b';
            case 0x01:      return 27;   // ESCAPE scancode

            default:
                if (scancode < KEY_LIMIT)
                {
                    bool shift = left_shift || right_shift;
                    char c = scancode_to_ascii(scancode, shift);
                    if (c != 0)
                    {
                        return c;
                    }
                }
                break;
        }
    }
}

// ============================================================================
// APPLICATIES & UTILITIES
// ============================================================================

/**
 * FopmaOS Mini Kladblok
 * Hiermee kun je live tekst typen en opslaan in het RAM-geheugen.
 */
void mini_editor()
{
    cls();
    print_string("--- FopmaOS Mini Kladblok --- (Druk op ESC om op te slaan & sluiten)\n", VGA_COLOR_LIGHT_MAGENTA);
    print_string("--------------------------------------------------------------------\n", VGA_COLOR_LIGHT_MAGENTA);

    // LAAD HET BESTAND: Als er al tekst in het geheugen stond, printen we dat eerst
    for (size_t i = 0; i < globale_tekst_buffer.size(); i++) 
    {
        print_char(globale_tekst_buffer[i]);
    }

    while (true)
    {
        char c = read_char(); 

        // 1. ESCAPE TOETS -> Opslaan en terug naar de shell
        if (c == 27) 
        {
            break; 
        }

        // 2. ENTER TOETS -> Volgende regel
        else if (c == '\n' || c == '\r') 
        {
            globale_tekst_buffer.push_back('\n');
            print_char('\n');
        }

        // 3. BACKSPACE TOETS -> Letter wissen uit geheugen en van scherm
        else if (c == '\b') 
        {
            if (globale_tekst_buffer.size() > 0) 
            {
                globale_tekst_buffer.pop_back(); // Verwijder uit RAM
                
                // BACKSPACE FIX: Stuur de juiste opeenvolging naar het scherm.
                // Dit werkt zodra je print_char() in 'screens.h' hebt aangepast voor '\b'.
                print_char('\b');
                print_char(' ');
                print_char('\b');
            }
        }

        // 4. NORMALE LETTERS EN CIJFERS -> Opslaan in RAM en live typen
        else if (c >= 32 && c <= 126) 
        {
            globale_tekst_buffer.push_back(c); 
            print_char(c);             
        }
    }

    // Na het drukken op ESC sluiten we af en gaan we terug naar de shell
    cls();
    print_string("Bestand succesvol bewaard in RAM-geheugen!\n", VGA_COLOR_LIGHT_GREEN);
}

// ============================================================================
// HARDWARE INTERACTIE (RTC / POWER)
// ============================================================================

int get_rtc_register(int reg) 
{
    outb(0x70, reg);
    return inb(0x71);
}

void print_current_time() 
{
    int sec = get_rtc_register(0);
    int min = get_rtc_register(2);
    int uur = get_rtc_register(4);

    // BCD omzetten naar normale ints
    sec = (sec & 0x0F) + ((sec / 16) * 10);
    min = (min & 0x0F) + ((min / 16) * 10);
    uur = (uur & 0x0F) + ((uur / 16) * 10);

    // TIJDZONE FIX: +2 voor Nederlandse zomertijd (+1 voor wintertijd)
    uur = uur + 2;

    if (uur >= 24) {
        uur = uur - 24;
    }

    if (uur < 10) print_string("0");
    print_int(uur);
    print_string(":");

    if (min < 10) print_string("0");
    print_int(min);
    print_string(":");

    if (sec < 10) print_string("0");
    print_int(sec);
    print_string("\n");
}

void sys_shutdown() 
{
    // QEMU ACPI Shutdown
    asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    asm volatile ("outw %0, %1" : : "a"((uint16_t)0x2000), "Nd"((uint16_t)0xB004));

    // Fallback als shutdown niet werkt
    cls();
    print_string("U kunt de computer nu veilig uitschakelen.\n", VGA_COLOR_LIGHT_GREY);
    while (true) 
    {
        asm volatile("hlt");
    }
}

void sys_reboot() 
{
    // Pulseert de reset-lijn via de keyboard controller
    outb(0x64, 0xFE);
    
    while (true) 
    {
        asm volatile("hlt");
    }
}

// ============================================================================
// SHELL HELPER FUNCTIES
// ============================================================================

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

// ============================================================================
// KERNEL MAIN ENTRY POINT
// ============================================================================

extern "C" void kernel_main(multiboot_info *mbi)
{
    // 1. Scherm leegmaken
    cls();

    // 2. Schone, VGA-proof opstartbanner
    print_string(
        "   ______                                 ____   _____ \n"
        "  |  ____|                               / __ \\ / ____|\n"
        "  | |__ ___  _ __  _ __ ___   __ _      | |  | | (___  \n"
        "  |  __/ _ \\| '_ \\| '_ ` _ \\ / _` |     | |  | |\\___ \\ \n"
        "  | | | (_) | |_) | | | | | | (_| |     | |__| |____) |\n"
        "  |_|  \\___/| .__/|_| |_| |_|\\__,_|      \\____/|_____/ \n"
        "            | |                                        \n"
        "            |_|                                        \n\n",
        VGA_COLOR_LIGHT_CYAN
    );

    // 3. Systeeminformatie
    print_string("Free Memory: ", VGA_COLOR_LIGHT_GREY);
    print_int(mbi->mem_lower + mbi->mem_upper);
    print_string(" KB\n\n");

    // 4. Shell-loop
    while (true)
    {
        vector<char> i;
        print_string("- ", VGA_COLOR_LIGHT_BLUE);
        input(i);
        print_char('\n');

        // COMMANDO: help
        if (is_command(i, "help"))
        {
            print_string(
                "FopmaOS - Beschikbare commando's:\n"
                "  help     - Toon dit menu\n"
                "  clear    - Maak het scherm leeg\n"
                "  editor   - Open het RAM Kladblok\n"
                "  read     - Toon de opgeslagen tekst uit het RAM\n"
                "  time     - Toon de huidige RTC tijd\n"
                "  version  - Toon OS versie\n"
                "  ping     - Test of de kernel reageert\n"
                "  reboot   - Start de computer opnieuw op\n"
                "  shutdown - Sluit het systeem af\n"
                "  contributers - View all the contributers and how to contribute to this project\n",
                VGA_COLOR_LIGHT_GREEN
            );
        }
        else if (is_command(i, "contributers")){
            print_string(
                "Contributer 1: Ethan. Source code maker.\n"
                "Github link contributer 1: https://github.com/EHowardHill\n"
                "Contributer 2: Sil Fopma. Owner of this repo and added more commands\n"
                "Github link contributer 2: https://github.com/SilFopma4h2\n"
                "To contribute to this project. Just write some usefull code and create a pull reqeust!\n"
                "Thanks you all for contributing to this project!\n",
                VGA_COLOR_LIGHT_GREEN
            );
        }
        // COMMANDO: editor
        else if (is_command(i, "editor"))
        {
            mini_editor();
        }

        // COMMANDO: read (Nieuw! Hiermee lees je de tekst direct uit de shell)
        else if (is_command(i, "read"))
        {
            if (globale_tekst_buffer.size() == 0) 
            {
                print_string("Het kladblok is momenteel leeg.\n", VGA_COLOR_LIGHT_GREY);
            } 
            else 
            {
                print_string("--- Inhoud van RAM-bestand ---\n", VGA_COLOR_LIGHT_MAGENTA);
                for (size_t j = 0; j < globale_tekst_buffer.size(); j++) 
                {
                    print_char(globale_tekst_buffer[j]);
                }
                print_string("\n------------------------------\n", VGA_COLOR_LIGHT_MAGENTA);
            }
        }

        // COMMANDO: clear
        else if (is_command(i, "clear") || is_command(i, "cls"))
        {
            cls();
        }
        //COMMANDO: fopfetch
        else if (is_command(i, "fopfetch"))
        {
            print_string(
                "   ______                                 ____   _____ \n"
                "  |  ____|                               / __ \\ / ____|\n"
                "  | |__ ___  _ __  _ __ ___   __ _      | |  | | (___  \n"
                "  |  __/ _ \\| '_ \\| '_ ` _ \\ / _` |     | |  | |\\___ \\ \n"
                "  | | | (_) | |_) | | | | | | (_| |     | |__| |____) |\n"
                "  |_|  \\___/| .__/|_| |_| |_|\\__,_|      \\____/|_____/ \n"
                "            | |                                        \n"
                "            |_|                                        \n\n",
                VGA_COLOR_LIGHT_CYAN
            );
            print_string(version, VGA_COLOR_LIGHT_CYAN);
            print_string("\n");
            print_string("Free Memory: ", VGA_COLOR_LIGHT_CYAN);
            print_int(mbi->mem_lower + mbi->mem_upper);
            print_string(" KB\n", VGA_COLOR_LIGHT_CYAN);
            print_string("\n");
            
            
        }
        // COMMANDO: ping
        else if (is_command(i, "ping"))
        {
            print_string("Pong!\n", VGA_COLOR_LIGHT_GREEN);
        }

        // COMMANDO: version
        else if (is_command(i, "version"))
        {
            print_string("FopmaOS version 0.1 Beta\n", VGA_COLOR_LIGHT_GREY);
        }

        // COMMANDO: time
        else if (is_command(i, "time"))
        {
            print_current_time();
        }

        // COMMANDO: reboot
        else if (is_command(i, "reboot"))
        {
            print_string("Het systeem wordt opnieuw opgestart...\n", VGA_COLOR_LIGHT_RED);
            sys_reboot();
        }

        // COMMANDO: shutdown
        else if (is_command(i, "shutdown"))
        {
            print_string("Je systeem wordt afgesloten...\n", VGA_COLOR_LIGHT_RED);
            sys_shutdown();
        }

        // FOUTMELDING
        else if (i.size() > 0)
        {
            print_string("Onbekend commando. Typ 'help' voor een overzicht.\n", VGA_COLOR_LIGHT_RED);
        }
    }
}