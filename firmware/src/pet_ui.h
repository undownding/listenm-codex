#ifndef PET_UI_H
#define PET_UI_H
int pet_ui_init(void);
void pet_ui_listening(int active,unsigned level);
void pet_ui_connected(int connected);
void pet_ui_event(const char *json);
#endif
