FN_STREAM_TEST_IO=1
.include "../src/platform/atari/fn_stream.s"
.import _mock_sio
.export _test_buf = fn_stream_buf
.export _test_frame = fn_stream_frame
.export _test_len = fn_stream_len
.export _test_mode = fn_stream_mode

