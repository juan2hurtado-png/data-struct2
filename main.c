#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_NAME_LENGTH 64
#define MAX_PHONE_LENGTH 32
#define MAX_DOCUMENT_LENGTH 32
#define MAX_CLASS_LENGTH 32
#define MAX_FLIGHT_TYPE_LENGTH 3
#define MAX_FLIGHT_CODE_LENGTH 8
#define MAX_LINE_LENGTH 128

#define NATIONAL_SEAT_START 1
#define NATIONAL_SEAT_END 250
#define FIRST_CLASS_START 1
#define FIRST_CLASS_END 20
#define ECONOMY_CLASS_START 21
#define ECONOMY_CLASS_END 250

#define NATIONAL_DURATION_MINUTES 50
#define INTERNATIONAL_DURATION_MINUTES (11 * 60)
#define INTERNATIONAL_TIME_DIFF_MINUTES (7 * 60)

typedef enum {
    FLIGHT_NATIONAL = 0,
    FLIGHT_INTERNATIONAL = 1
} FlightType;

typedef enum {
    CLASS_FIRST = 0,
    CLASS_ECONOMY = 1
} TicketClass;

typedef struct {
    int day;
    int month;
    int year;
} Date;

typedef struct {
    int hour;
    int minute;
} TimeOfDay;

typedef struct Passenger {
    FlightType flightType;
    char flightCode[MAX_FLIGHT_CODE_LENGTH];
    char document[MAX_DOCUMENT_LENGTH];
    char firstName[MAX_NAME_LENGTH];
    char lastName[MAX_NAME_LENGTH];
    char phone[MAX_PHONE_LENGTH];
    Date birthDate;
    char gender;
    TicketClass ticketClass;
    Date flightDate;
    TimeOfDay departureTime;
    Date arrivalDate;
    TimeOfDay arrivalTime;
    int seatNumber;
    struct Passenger *next;
} Passenger;

static const char *FLIGHT_CODES[] = {"GOPLA01", "GOPLA02"};
static const char *FLIGHT_TYPE_LABELS[] = {"01", "02"};
static const char *CLASS_LABELS[] = {"Primera Clase", "Clase Económica"};

static bool seatMap[2][ECONOMY_CLASS_END + 1];

static void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
    }
}

static void read_line(const char *prompt, char *buffer, size_t size) {
    while (1) {
        printf("%s", prompt);
        if (!fgets(buffer, (int)size, stdin)) {
            clearerr(stdin);
            continue;
        }
        trim_newline(buffer);
        if (strlen(buffer) == 0) {
            printf("Entrada vacía, intente de nuevo.\n");
            continue;
        }
        return;
    }
}

static int days_in_month(int month, int year) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2) {
        bool leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        return leap ? 29 : 28;
    }
    if (month < 1 || month > 12) return 0;
    return days[month - 1];
}

static bool parse_date(const char *input, Date *out) {
    if (!input || !out) return false;
    int day, month, year;
    if (sscanf(input, "%d/%d/%d", &day, &month, &year) != 3) {
        return false;
    }
    if (year < 1900 || month < 1 || month > 12) return false;
    int dim = days_in_month(month, year);
    if (day < 1 || day > dim) return false;
    out->day = day;
    out->month = month;
    out->year = year;
    return true;
}

static bool parse_time(const char *input, TimeOfDay *out) {
    if (!input || !out) return false;
    int hour, minute;
    if (sscanf(input, "%d:%d", &hour, &minute) != 2) {
        return false;
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return false;
    }
    out->hour = hour;
    out->minute = minute;
    return true;
}

static time_t datetime_to_time_t(Date date, TimeOfDay timeOfDay) {
    struct tm tmValue = {0};
    tmValue.tm_year = date.year - 1900;
    tmValue.tm_mon = date.month - 1;
    tmValue.tm_mday = date.day;
    tmValue.tm_hour = timeOfDay.hour;
    tmValue.tm_min = timeOfDay.minute;
    tmValue.tm_sec = 0;
    tmValue.tm_isdst = -1;
    return mktime(&tmValue);
}

static void time_t_to_datetime(time_t value, Date *date, TimeOfDay *timeOfDay) {
    struct tm *tmValue = localtime(&value);
    if (!tmValue) {
        return;
    }
    if (date) {
        date->day = tmValue->tm_mday;
        date->month = tmValue->tm_mon + 1;
        date->year = tmValue->tm_year + 1900;
    }
    if (timeOfDay) {
        timeOfDay->hour = tmValue->tm_hour;
        timeOfDay->minute = tmValue->tm_min;
    }
}

static void format_date(Date date, char *buffer, size_t size) {
    snprintf(buffer, size, "%02d/%02d/%04d", date.day, date.month, date.year);
}

static void format_time(TimeOfDay timeOfDay, char *buffer, size_t size) {
    snprintf(buffer, size, "%02d:%02d", timeOfDay.hour, timeOfDay.minute);
}

static bool is_future_or_present(Date date, TimeOfDay timeOfDay) {
    time_t now = time(NULL);
    time_t target = datetime_to_time_t(date, timeOfDay);
    return difftime(target, now) >= 0;
}

static bool is_past(Date date, TimeOfDay timeOfDay) {
    time_t now = time(NULL);
    time_t target = datetime_to_time_t(date, timeOfDay);
    return difftime(target, now) <= 0;
}

static Passenger *find_passenger(Passenger *head, const char *document) {
    for (Passenger *current = head; current; current = current->next) {
        if (strcmp(current->document, document) == 0) {
            return current;
        }
    }
    return NULL;
}

static int seat_range_start(TicketClass ticketClass) {
    return ticketClass == CLASS_FIRST ? FIRST_CLASS_START : ECONOMY_CLASS_START;
}

static int seat_range_end(TicketClass ticketClass) {
    return ticketClass == CLASS_FIRST ? FIRST_CLASS_END : ECONOMY_CLASS_END;
}

static int assign_random_seat(FlightType type, TicketClass ticketClass) {
    int start = seat_range_start(ticketClass);
    int end = seat_range_end(ticketClass);
    int available = 0;
    for (int i = start; i <= end; ++i) {
        if (!seatMap[type][i]) {
            available++;
        }
    }
    if (available == 0) {
        return -1;
    }
    int attempts = 0;
    while (attempts < 1000) {
        int seat = start + rand() % (end - start + 1);
        if (!seatMap[type][seat]) {
            seatMap[type][seat] = true;
            return seat;
        }
        attempts++;
    }
    /* Fall back to sequential search */
    for (int seat = start; seat <= end; ++seat) {
        if (!seatMap[type][seat]) {
            seatMap[type][seat] = true;
            return seat;
        }
    }
    return -1;
}

static void release_seat(FlightType type, int seatNumber) {
    if (seatNumber < FIRST_CLASS_START || seatNumber > ECONOMY_CLASS_END) {
        return;
    }
    seatMap[type][seatNumber] = false;
}

static void compute_arrival(FlightType type, Date departureDate, TimeOfDay departureTime,
                            Date *arrivalDate, TimeOfDay *arrivalTime) {
    int minutesToAdd = 0;
    if (type == FLIGHT_NATIONAL) {
        minutesToAdd = NATIONAL_DURATION_MINUTES;
    } else {
        minutesToAdd = INTERNATIONAL_DURATION_MINUTES + INTERNATIONAL_TIME_DIFF_MINUTES;
    }
    time_t departure = datetime_to_time_t(departureDate, departureTime);
    time_t arrival = departure + minutesToAdd * 60;
    time_t_to_datetime(arrival, arrivalDate, arrivalTime);
}

static void display_passenger(Passenger *passenger, bool includeFlightDetails) {
    if (!passenger) return;
    char birthBuffer[16];
    char flightDate[16];
    char flightTime[8];
    char arrivalDate[16];
    char arrivalTime[8];

    format_date(passenger->birthDate, birthBuffer, sizeof(birthBuffer));
    format_date(passenger->flightDate, flightDate, sizeof(flightDate));
    format_time(passenger->departureTime, flightTime, sizeof(flightTime));
    format_date(passenger->arrivalDate, arrivalDate, sizeof(arrivalDate));
    format_time(passenger->arrivalTime, arrivalTime, sizeof(arrivalTime));

    printf("Documento: %s\n", passenger->document);
    printf("Nombre: %s\n", passenger->firstName);
    printf("Apellido: %s\n", passenger->lastName);
    printf("Teléfono: %s\n", passenger->phone);
    printf("Fecha de nacimiento: %s\n", birthBuffer);
    printf("Género: %c\n", passenger->gender);
    printf("Clase de tiquete: %s\n", CLASS_LABELS[passenger->ticketClass]);
    printf("Silla: %d\n", passenger->seatNumber);
    if (includeFlightDetails) {
        printf("Tipo de vuelo: %s\n", FLIGHT_TYPE_LABELS[passenger->flightType]);
        printf("Código de vuelo: %s\n", passenger->flightCode);
        printf("Fecha de vuelo: %s\n", flightDate);
        printf("Hora de salida: %s\n", flightTime);
        printf("Fecha de llegada: %s\n", arrivalDate);
        printf("Hora de llegada: %s\n", arrivalTime);
    }
}

static FlightType read_flight_type(void) {
    char buffer[MAX_LINE_LENGTH];
    while (1) {
        printf("Seleccione el tipo de vuelo:\n");
        printf("01. Nacional (Pereira-Bogotá)\n");
        printf("02. Internacional (Bogotá-Madrid)\n");
        read_line("Opción: ", buffer, sizeof(buffer));
        if (strcmp(buffer, "01") == 0 || strcmp(buffer, "1") == 0) {
            return FLIGHT_NATIONAL;
        }
        if (strcmp(buffer, "02") == 0 || strcmp(buffer, "2") == 0) {
            return FLIGHT_INTERNATIONAL;
        }
        printf("Opción inválida. Intente nuevamente.\n");
    }
}

static TicketClass read_ticket_class(void) {
    char buffer[MAX_LINE_LENGTH];
    while (1) {
        printf("Seleccione la clase de tiquete:\n");
        printf("1. Primera Clase (sillas 1-20)\n");
        printf("2. Clase Económica (sillas 21-250)\n");
        read_line("Opción: ", buffer, sizeof(buffer));
        if (strcmp(buffer, "1") == 0) {
            return CLASS_FIRST;
        }
        if (strcmp(buffer, "2") == 0) {
            return CLASS_ECONOMY;
        }
        printf("Opción inválida. Intente nuevamente.\n");
    }
}

static char read_gender(void) {
    char buffer[MAX_LINE_LENGTH];
    while (1) {
        read_line("Género (F/M/O): ", buffer, sizeof(buffer));
        if (strlen(buffer) != 1) {
            printf("Ingrese únicamente F, M u O.\n");
            continue;
        }
        char gender = (char)toupper((unsigned char)buffer[0]);
        if (gender == 'F' || gender == 'M' || gender == 'O') {
            return gender;
        }
        printf("Valor inválido.\n");
    }
}

static void read_birth_date(Date *out) {
    char buffer[MAX_LINE_LENGTH];
    TimeOfDay midnight = {0, 0};
    while (1) {
        read_line("Fecha de nacimiento (dd/mm/aaaa): ", buffer, sizeof(buffer));
        if (!parse_date(buffer, out)) {
            printf("Fecha inválida.\n");
            continue;
        }
        if (!is_past(*out, midnight)) {
            printf("La fecha de nacimiento debe ser en el pasado.\n");
            continue;
        }
        return;
    }
}

static void read_flight_datetime(Date *date, TimeOfDay *timeOfDay) {
    char buffer[MAX_LINE_LENGTH];
    while (1) {
        read_line("Fecha del vuelo (dd/mm/aaaa): ", buffer, sizeof(buffer));
        if (!parse_date(buffer, date)) {
            printf("Fecha inválida.\n");
            continue;
        }
        read_line("Hora de salida (hh:mm, formato 24 horas): ", buffer, sizeof(buffer));
        if (!parse_time(buffer, timeOfDay)) {
            printf("Hora inválida.\n");
            continue;
        }
        if (!is_future_or_present(*date, *timeOfDay)) {
            printf("La fecha y hora del vuelo deben ser presentes o futuras.\n");
            continue;
        }
        return;
    }
}

static void add_passenger(Passenger **head, Passenger *newPassenger) {
    if (!*head) {
        *head = newPassenger;
    } else {
        Passenger *current = *head;
        while (current->next) {
            current = current->next;
        }
        current->next = newPassenger;
    }
}

static void buy_ticket(Passenger **head) {
    Passenger *newPassenger = (Passenger *)calloc(1, sizeof(Passenger));
    if (!newPassenger) {
        printf("No se pudo reservar memoria para el pasajero.\n");
        return;
    }

    FlightType flightType = read_flight_type();
    newPassenger->flightType = flightType;
    strncpy(newPassenger->flightCode, FLIGHT_CODES[flightType], sizeof(newPassenger->flightCode));
    newPassenger->flightCode[sizeof(newPassenger->flightCode) - 1] = '\0';

    char buffer[MAX_LINE_LENGTH];
    while (1) {
        read_line("Documento del pasajero: ", buffer, sizeof(buffer));
        if (find_passenger(*head, buffer)) {
            printf("Ya existe un pasajero con ese documento.\n");
            continue;
        }
        strncpy(newPassenger->document, buffer, sizeof(newPassenger->document));
        newPassenger->document[sizeof(newPassenger->document) - 1] = '\0';
        break;
    }

    read_line("Nombre del pasajero: ", buffer, sizeof(buffer));
    strncpy(newPassenger->firstName, buffer, sizeof(newPassenger->firstName));
    newPassenger->firstName[sizeof(newPassenger->firstName) - 1] = '\0';

    read_line("Apellido del pasajero: ", buffer, sizeof(buffer));
    strncpy(newPassenger->lastName, buffer, sizeof(newPassenger->lastName));
    newPassenger->lastName[sizeof(newPassenger->lastName) - 1] = '\0';

    read_line("Teléfono del pasajero: ", buffer, sizeof(buffer));
    strncpy(newPassenger->phone, buffer, sizeof(newPassenger->phone));
    newPassenger->phone[sizeof(newPassenger->phone) - 1] = '\0';

    read_birth_date(&newPassenger->birthDate);
    newPassenger->gender = read_gender();
    newPassenger->ticketClass = read_ticket_class();

    read_flight_datetime(&newPassenger->flightDate, &newPassenger->departureTime);

    compute_arrival(newPassenger->flightType, newPassenger->flightDate, newPassenger->departureTime,
                    &newPassenger->arrivalDate, &newPassenger->arrivalTime);

    int seat = assign_random_seat(newPassenger->flightType, newPassenger->ticketClass);
    if (seat == -1) {
        printf("No hay sillas disponibles en la clase seleccionada para este vuelo.\n");
        free(newPassenger);
        return;
    }
    newPassenger->seatNumber = seat;
    newPassenger->next = NULL;

    add_passenger(head, newPassenger);
    printf("Tiquete comprado exitosamente. Silla asignada: %d\n", seat);
}

static void modify_passenger(Passenger *head) {
    char buffer[MAX_LINE_LENGTH];
    read_line("Documento del pasajero a modificar: ", buffer, sizeof(buffer));
    Passenger *passenger = find_passenger(head, buffer);
    if (!passenger) {
        printf("No se encontró un pasajero con ese documento.\n");
        return;
    }

    printf("Modificando pasajero %s %s\n", passenger->firstName, passenger->lastName);
    read_line("Nuevo nombre: ", buffer, sizeof(buffer));
    strncpy(passenger->firstName, buffer, sizeof(passenger->firstName));
    passenger->firstName[sizeof(passenger->firstName) - 1] = '\0';

    read_line("Nuevo apellido: ", buffer, sizeof(buffer));
    strncpy(passenger->lastName, buffer, sizeof(passenger->lastName));
    passenger->lastName[sizeof(passenger->lastName) - 1] = '\0';

    read_line("Nuevo teléfono: ", buffer, sizeof(buffer));
    strncpy(passenger->phone, buffer, sizeof(passenger->phone));
    passenger->phone[sizeof(passenger->phone) - 1] = '\0';

    read_birth_date(&passenger->birthDate);
    passenger->gender = read_gender();

    printf("Datos modificados correctamente.\n");
}

static void list_passengers(Passenger *head) {
    if (!head) {
        printf("No hay pasajeros registrados.\n");
        return;
    }
    for (Passenger *current = head; current; current = current->next) {
        display_passenger(current, false);
        printf("-----------------------------\n");
    }
}

static void search_passenger(Passenger *head) {
    char buffer[MAX_LINE_LENGTH];
    read_line("Documento del pasajero a buscar: ", buffer, sizeof(buffer));
    Passenger *passenger = find_passenger(head, buffer);
    if (!passenger) {
        printf("No se encontró un pasajero con ese documento.\n");
        return;
    }
    display_passenger(passenger, true);
}

static void show_available_seats(FlightType type, TicketClass ticketClass) {
    int start = seat_range_start(ticketClass);
    int end = seat_range_end(ticketClass);
    printf("Sillas disponibles: ");
    int count = 0;
    for (int seat = start; seat <= end; ++seat) {
        if (!seatMap[type][seat]) {
            printf("%d ", seat);
            count++;
            if (count % 15 == 0) {
                printf("\n");
            }
        }
    }
    if (count == 0) {
        printf("(ninguna)");
    }
    printf("\n");
}

static void change_seat(Passenger *head) {
    char buffer[MAX_LINE_LENGTH];
    read_line("Documento del pasajero: ", buffer, sizeof(buffer));
    Passenger *passenger = find_passenger(head, buffer);
    if (!passenger) {
        printf("No se encontró un pasajero con ese documento.\n");
        return;
    }

    printf("Silla actual: %d\n", passenger->seatNumber);
    show_available_seats(passenger->flightType, passenger->ticketClass);
    printf("Ingrese la nueva silla deseada: ");
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        printf("Entrada inválida.\n");
        return;
    }
    trim_newline(buffer);
    char *endPtr = NULL;
    long seatValue = strtol(buffer, &endPtr, 10);
    if (endPtr == buffer || *endPtr != '\0') {
        printf("Número de silla inválido.\n");
        return;
    }
    int seat = (int)seatValue;
    int start = seat_range_start(passenger->ticketClass);
    int end = seat_range_end(passenger->ticketClass);
    if (seat < start || seat > end) {
        printf("La silla seleccionada no pertenece a la clase del pasajero.\n");
        return;
    }
    if (seatMap[passenger->flightType][seat]) {
        printf("La silla seleccionada no está disponible.\n");
        return;
    }
    release_seat(passenger->flightType, passenger->seatNumber);
    seatMap[passenger->flightType][seat] = true;
    passenger->seatNumber = seat;
    printf("Silla actualizada correctamente.\n");
}

static void print_boarding_pass(Passenger *head) {
    char buffer[MAX_LINE_LENGTH];
    read_line("Documento del pasajero: ", buffer, sizeof(buffer));
    Passenger *passenger = find_passenger(head, buffer);
    if (!passenger) {
        printf("No se encontró un pasajero con ese documento.\n");
        return;
    }

    char flightDate[16];
    char departureTime[8];
    char arrivalDate[16];
    char arrivalTime[8];

    format_date(passenger->flightDate, flightDate, sizeof(flightDate));
    format_time(passenger->departureTime, departureTime, sizeof(departureTime));
    format_date(passenger->arrivalDate, arrivalDate, sizeof(arrivalDate));
    format_time(passenger->arrivalTime, arrivalTime, sizeof(arrivalTime));

    printf("/////////////GOLONDRINA VELOZ//////////////////////////////\n");
    printf("///////////////////////PASE DE ABORDAR/////////////////////\n");
    printf("Tipo vuelo: %s\n", FLIGHT_TYPE_LABELS[passenger->flightType]);
    printf("Código vuelo: %s\n", passenger->flightCode);
    printf("Documento pasajero: %s\n", passenger->document);
    printf("Nombre pasajero: %s\n", passenger->firstName);
    printf("Apellido pasajero: %s\n", passenger->lastName);
    printf("Clase de tiquete: %s\n", CLASS_LABELS[passenger->ticketClass]);
    printf("Fecha vuelo: %s\n", flightDate);
    printf("Hora salida: %s\n", departureTime);
    printf("Fecha llegada: %s\n", arrivalDate);
    printf("Hora llegada: %s\n", arrivalTime);
    printf("Silla: %d\n", passenger->seatNumber);
}

static void cancel_ticket(Passenger **head) {
    char buffer[MAX_LINE_LENGTH];
    read_line("Documento del pasajero a cancelar: ", buffer, sizeof(buffer));

    Passenger *current = *head;
    Passenger *previous = NULL;
    while (current) {
        if (strcmp(current->document, buffer) == 0) {
            if (previous) {
                previous->next = current->next;
            } else {
                *head = current->next;
            }
            release_seat(current->flightType, current->seatNumber);
            free(current);
            printf("Tiquete cancelado correctamente.\n");
            return;
        }
        previous = current;
        current = current->next;
    }
    printf("No se encontró un pasajero con ese documento.\n");
}

static void free_passengers(Passenger *head) {
    while (head) {
        Passenger *next = head->next;
        free(head);
        head = next;
    }
}

static void print_menu(void) {
    printf("/////////////GOLONDRINA VELOZ//////////////////////////////\n");
    printf("///////////////////////TIQUETES///////////////////////////////////////////\n");
    printf("1. Comprar Tiquete\n");
    printf("2. Modificar Pasajero\n");
    printf("3. Listar Pasajeros\n");
    printf("4. Buscar pasajero\n");
    printf("5. Cambiar Silla\n");
    printf("6. Imprimir pase de abordar\n");
    printf("7. Cancelar Tiquete\n");
    printf("8. Salir\n");
}

int main(void) {
    srand((unsigned int)time(NULL));
    Passenger *head = NULL;
    char buffer[MAX_LINE_LENGTH];

    while (1) {
        print_menu();
        read_line("Seleccione una opción: ", buffer, sizeof(buffer));
        int option = atoi(buffer);
        switch (option) {
            case 1:
                buy_ticket(&head);
                break;
            case 2:
                modify_passenger(head);
                break;
            case 3:
                list_passengers(head);
                break;
            case 4:
                search_passenger(head);
                break;
            case 5:
                change_seat(head);
                break;
            case 6:
                print_boarding_pass(head);
                break;
            case 7:
                cancel_ticket(&head);
                break;
            case 8:
                free_passengers(head);
                printf("Gracias por utilizar el sistema de tiquetes.\n");
                return 0;
            default:
                printf("Opción inválida, intente nuevamente.\n");
        }
        printf("\n");
    }
}

