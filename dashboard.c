#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
int _getch(void) {
    struct termios oldt, newt;
    int ch;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    return ch;
}
#endif

// ANSI Color Codes
#define COLOR_RESET   "\x1b[0m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define BG_CYAN_FG_BLACK "\x1b[46;30m"

void clear_screen() {
    // using ANSI escape codes for instantaneous clearing
    // this avoids forking a new shell and completely cures terminal flicker
    printf("\x1b[2J\x1b[H");
    fflush(stdout);
}

void print_banner() {
    printf(COLOR_CYAN);
    printf("========================================================\n");
    printf(" Federated Learning Parameter Server - Interactive Demo \n");
    printf("========================================================\n");
    printf(COLOR_RESET);
}

// Returns 1 for UP, 2 for DOWN, 3 for ENTER, or raw char
int get_key() {
    int c = _getch();
#ifdef _WIN32
    if (c == 0 || c == 224) {
        int c2 = _getch();
        if (c2 == 72) return 1; // UP
        if (c2 == 80) return 2; // DOWN
    }
    if (c == '\r' || c == '\n') return 3; // ENTER
#else
    if (c == 27) {
        int c2 = _getch();
        int c3 = _getch();
        if (c2 == '[') {
            if (c3 == 'A') return 1; // UP
            if (c3 == 'B') return 2; // DOWN
        }
        return 0; // Unknown escape
    }
    if (c == '\n' || c == '\r') return 3; // ENTER
#endif
    return c;
}

int main() {
    char server_ip[256];
    char command_buf[512];
    int rounds;
    
    strcpy(server_ip, "127.0.0.1"); // Default loopback

    const char* main_options[] = {
        " Edge Node (Push Gradients) ",
        " Guest Client (View Model Weights) ",
        " Admin Access (Reset/Shutdown Server) ",
        " Change Server IP ",
        " Exit "
    };
    int num_main_opts = 5;
    int main_selected = 0;

    while (1) {
        // --- Main Menu Render Loop ---
        while (1) {
            clear_screen();
            print_banner();
            printf(COLOR_RED "\nIMPORTANT: Please ensure you have run './server'\n");
            printf("in a SEPARATE terminal window before proceeding.\n\n" COLOR_RESET);
            printf(COLOR_YELLOW "Current Server IP: " COLOR_GREEN "%s\n\n" COLOR_RESET, server_ip);
            
            printf("Select Role to Simulate (Use " COLOR_CYAN "UP/DOWN" COLOR_RESET " arrows and " COLOR_CYAN "ENTER" COLOR_RESET "):\n\n");
            
            for (int i = 0; i < num_main_opts; i++) {
                if (i == main_selected) {
                    printf("  " BG_CYAN_FG_BLACK " > %s " COLOR_RESET "\n", main_options[i]);
                } else {
                    printf("     %s \n", main_options[i]);
                }
            }
            
            int key = get_key();
            if (key == 1) { // UP
                main_selected--;
                if (main_selected < 0) main_selected = num_main_opts - 1;
            } else if (key == 2) { // DOWN
                main_selected++;
                if (main_selected >= num_main_opts) main_selected = 0;
            } else if (key == 3) { // ENTER
                break;
            }
        }
        
        clear_screen();
        
        if (main_selected == 0) {
            printf(COLOR_GREEN "--- Edge Node Simulation ---\n" COLOR_RESET);
            printf("Enter number of training rounds to push (e.g., 2): ");
            if (scanf("%d", &rounds) != 1) {
                while(getchar() != '\n');
                rounds = 1;
            } else {
                while(getchar() != '\n'); // clear newline from scanf so it doesn't mess with next getch
            }
            sprintf(command_buf, "./client_node %s %d", server_ip, rounds);
            printf(COLOR_YELLOW "\nExecuting: %s\n" COLOR_RESET, command_buf);
            system(command_buf);
            
            printf("\nPress Enter to return to menu...");
            get_key();
            
        } else if (main_selected == 1) {
            printf(COLOR_GREEN "--- Guest Client Simulation ---\n" COLOR_RESET);
            sprintf(command_buf, "./guest_client %s", server_ip);
            printf(COLOR_YELLOW "\nExecuting: %s\n" COLOR_RESET, command_buf);
            system(command_buf);
            
            printf("\nPress Enter to return to menu...");
            get_key();
            
        } else if (main_selected == 2) {
            const char* admin_opts[] = {
                " Reset Model Weights to Zero ",
                " Shutdown Server Safely ",
                " Cancel "
            };
            int admin_selected = 0;
            
            while (1) {
                clear_screen();
                printf(COLOR_RED "--- Admin Access ---\n\n" COLOR_RESET);
                printf("Choose command (Use " COLOR_CYAN "UP/DOWN" COLOR_RESET " arrows and " COLOR_CYAN "ENTER" COLOR_RESET "):\n\n");
                
                for (int i = 0; i < 3; i++) {
                    if (i == admin_selected) {
                        printf("  " BG_CYAN_FG_BLACK " > %s " COLOR_RESET "\n", admin_opts[i]);
                    } else {
                        printf("     %s \n", admin_opts[i]);
                    }
                }
                
                int key = get_key();
                if (key == 1) { admin_selected--; if (admin_selected < 0) admin_selected = 2; }
                else if (key == 2) { admin_selected++; if (admin_selected >= 3) admin_selected = 0; }
                else if (key == 3) break;
            }
            
            clear_screen();
            if (admin_selected == 0) {
                sprintf(command_buf, "./admin_client %s reset", server_ip);
                printf(COLOR_YELLOW "Executing: %s\n" COLOR_RESET, command_buf);
                system(command_buf);
            } else if (admin_selected == 1) {
                sprintf(command_buf, "./admin_client %s shutdown", server_ip);
                printf(COLOR_YELLOW "Executing: %s\n" COLOR_RESET, command_buf);
                system(command_buf);
                printf(COLOR_RED "\nNote: Server shutdown sent. You may need to restart the server.\n" COLOR_RESET);
            }
            
            if (admin_selected != 2) {
                printf("\nPress Enter to return to menu...");
                get_key();
            }
            
        } else if (main_selected == 3) {
            printf(COLOR_YELLOW "Enter new server IP address" COLOR_RESET " (current: %s): ", server_ip);
            if (scanf("%255s", server_ip) == 1) {
                while(getchar() != '\n'); 
            }
        } else if (main_selected == 4) {
            printf(COLOR_CYAN "\nExiting dashboard.\n" COLOR_RESET);
            return 0;
        }
    }

    return 0;
}
