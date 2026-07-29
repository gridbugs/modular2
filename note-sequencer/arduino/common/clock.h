#ifdef SLOW_ARDUINO
// Some cheap arduino clones seem to run at a quarter of the expected speed
// (4MHz rather than 16MHz).
#define OSC_HZ 4000000L
#else
#define OSC_HZ 16000000L
#endif
