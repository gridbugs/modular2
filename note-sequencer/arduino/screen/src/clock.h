#ifdef SLOW_ARDUINO
// Some cheap arduino clones seem to run at a quarter of the expected speed
// (4MHz rather than 16MHz).
#define OSC_HZ 4000000
#else
#define OSC_HZ 16000000
#endif
