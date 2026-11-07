#include "command.h"
#include "state.h"

void state_init(state_t *state) {
  state->mode = MODE_PROGRAM_OR_LIVE;
  state->clock_source = CLOCK_SOURCE_EXTERNAL;
  state->clock_state = false;
  state->sequence_length = MAX_SEQUENCE_LENGTH;
  state->playback_index = 0;
  state->edit_index = 0;
  state->tempo_bpm = 60;
  state->ticks_per_beat = 4;
  state->setting_field = SETTING_FIELD_NONE;
  state->gate_duration_ratio = 127;
  state->glide_duration_ratio = 16;
  state->octave = 1;
  state->live_flags = 0;
  state->edit_mode = EDIT_MODE_IN_PLACE;
  state->accent_type = ACCENT_TYPE_FLAT;
  state->accent_duration_ratio = 127;
  state->accent_size = 255;
}

command_t state_command_add_to_playback_index(state_t *state, int8_t delta) {
  int8_t new_playback_index = (((int8_t)state->playback_index) + delta);
  while (new_playback_index < 0) {
    new_playback_index += state->sequence_length;
  }
  return command_set_playback_index(new_playback_index % state->sequence_length);
}

command_t state_command_add_to_edit_index(state_t *state, int8_t delta) {
  int8_t new_edit_index = (((int8_t)state->edit_index) + delta);
  while (new_edit_index < 0) {
    new_edit_index += state->sequence_length;
  }
  return command_set_edit_index(new_edit_index % state->sequence_length);
}

command_t state_command_toggle_flags(state_t *state, uint8_t flags) {
  if (state->mode == MODE_PROGRAM_OR_LIVE && state->live) {
    state->live_flags ^= flags;
    return command_set_live_flags(state->live_flags);
  } else {
    step_t *step = state_current_edit_step_ptr(state);
    step->flags ^= flags;
    return command_set_step_flags(state->edit_index, step->flags);
  }
}

void state_clear_sequence(state_t *state) {
  state->edit_index = 0;
  state->playback_index = 0;
  for (int i = 0; i < MAX_SEQUENCE_LENGTH; i++) {
    step_t *step = &state->sequence.steps[i];
    step->enabled = false;
    step->flags = 0;
  }
}

step_t *state_current_edit_step_ptr(state_t *state) {
  return &state->sequence.steps[state->edit_index];
}

step_t state_current_playback_step(state_t *state) {
  return state->sequence.steps[state->playback_index];
}

bool state_current_has_glide(state_t *state) {
  if (state_is_live(state)) {
    return state_live_has_glide(state);
  } else {
    return state_current_playback_step(state).flags & FLAG_GLIDE;
  }
}

bool state_current_has_accent(state_t *state) {
  if (state_is_live(state)) {
    return state_live_has_accent(state);
  } else {
    return state_current_playback_step(state).flags & FLAG_ACCENT;
  }
}

static void set_field(state_t *state, setting_field_t setting_field, uint8_t value) {
  switch (setting_field) {
    case SETTING_FIELD_TEMPO_BPM:
      state->tempo_bpm = value;
      break;
    case SETTING_FIELD_GATE_DURATION_RATIO:
      state->gate_duration_ratio = value;
      break;
    case SETTING_FIELD_GLIDE_DURATION_RATIO:
      state->glide_duration_ratio = value;
      break;
    case SETTING_FIELD_ACCENT_DURATION_RATIO:
      state->accent_duration_ratio = value;
      break;
    case SETTING_FIELD_ACCENT_SIZE:
      state->accent_size = value;
      break;
    case SETTING_FIELD_SEQUENCE_LENGTH:
      state->sequence_length = value;
      break;
    default:
    case SETTING_FIELD_NONE:
      break;
  }
}

void state_apply_command(state_t *state, command_t command) {
  switch (command.typ) {
    case COMMAND_HELLO:
    case COMMAND_SHOW_SPLASH:
    case COMMAND_SHOW_UI:
    case COMMAND_SET_NOTE:
      break;
    case COMMAND_SET_PLAYBACK_INDEX:
      state->playback_index = command.args.set_playback_index.sequence_index;
      break;
    case COMMAND_SET_EDIT_INDEX:
      state->edit_index = command.args.set_edit_index.sequence_index;
      break;
    case COMMAND_SET_STEP_NOTE: {
      uint8_t sequence_index = command.args.set_step_note.sequence_index;
      step_t *step = &state->sequence.steps[sequence_index];
      step->note_index = command.args.set_step_note.note_index;
      step->enabled = true;
      break;
    }
    case COMMAND_CLEAR_STEP_NOTE: {
      uint8_t sequence_index = command.args.set_step_note.sequence_index;
      step_t *step = &state->sequence.steps[sequence_index];
      step->enabled = false;
      break;
    }
    case COMMAND_SET_STEP_FLAGS: {
      uint8_t sequence_index = command.args.set_step_flags.sequence_index;
      step_t *step = &state->sequence.steps[sequence_index];
      step->flags = command.args.set_step_flags.flags;
      break;
    }
    case COMMAND_SET_LIVE_FLAGS:
      state->live_flags = command.args.set_live_flags.flags;
      break;
    case COMMAND_CLEAR_SEQUENCE:
      state_clear_sequence(state);
      break;
    case COMMAND_SET_MODE:
      state->mode = command.args.set_mode.mode;
      break;
    case COMMAND_SET_LIVE:
      state->live = command.args.set_live.live;
      break;
    case COMMAND_SETTING_FIELD:
      state->setting_field = command.args.setting_field.setting_field;
      break;
    case COMMAND_SET_FIELD:
      set_field(state, command.args.set_field.setting_field, command.args.set_field.value);
      break;
    case COMMAND_SET_CLOCK:
      state->clock_state = command.args.set_clock.clock_state;
      break;
    case COMMAND_SET_CLOCK_SOURCE:
      state->clock_source = command.args.set_clock_source.clock_source;
      break;
    case COMMAND_SET_OCTAVE:
      state->octave = command.args.set_octave.octave;
      break;
    case COMMAND_SET_EDIT_MODE:
      state->edit_mode = command.args.set_edit_mode.edit_mode;
      break;
    case COMMAND_SET_ACCENT_TYPE:
      state->accent_type = command.args.set_accent_type.accent_type;
      break;
  }
}
