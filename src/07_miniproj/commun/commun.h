#define FREQ_MAX 20
#define FREQ_MIN 1

#define clamp(val, min, max) \
    ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

static const char module_name[] = "my_module";
static const char module_conf_path[] = "/sys/class/my_module/my_module/config";

typedef enum {
    MODE_AUTO,
    MODE_MANUAL
} module_mode_t;

typedef struct commun {
    module_mode_t mode;
    int frequency;
    int duty;
} module_config_t;

static const char* const str_mode_option[] = {"auto", "manual", NULL};