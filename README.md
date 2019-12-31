# vumeter 2.0

This is a true VU meter for PulseAudio. (Most so-called VU meters are in
fact peak program meters with a damped decay.)

A VU meter's reading corresponds approximately to the perceptual response of
the human ear. This implementation differs from an analog VU meter in that
it has a wider effective dynamic range and a full-scale value corresponding
to digital 0 dB. The meter's ballistic property matches an analog VU meter
in that it takes the needle approximately 300 milliseconds to go from
no-signal to 0 dB upon application of a full-scale signal.

It is important that you understand that this meter is not suited to setting
levels for a digital recording; use a peak program meter. This meter is best
suited for evaluating perceptual program levels during playback.

In addition to the meter, two indicators report SIGNAL and PEAK at levels
greater than -56 dB and -1.5 dB, respectively.

The PEAK indicator is provided to detect peaks in the program material which
naturally occur as a result of normalizing the audio to a value approaching
0 dB; when the average volume is below 0 dB, peaks will still occur in excess
of that average value. You should not use this indicator to set recording
levels.

This code descends from original work by Martin Cameron. I've added text for
labels on the scale and a logo, and have reworked the initialization code to
(IMO) improve readability.

The program requires the SDL2 and SDL2\_ttf libraries and the DejaVu fonts.
