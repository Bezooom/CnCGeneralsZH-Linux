#ifndef PAL_INPUT_H
#define PAL_INPUT_H

#include "GameClient/Keyboard.h"

class LinuxKeyboard : public Keyboard
{
public:
	LinuxKeyboard();
	virtual ~LinuxKeyboard();

	virtual Bool getCapsState() override;
	void addKeyEvent(UnsignedByte keyCode, Bool isDown);

protected:
	virtual void getKey(KeyboardIO *key) override;

private:
	enum { EVENT_BUFFER_SIZE = 256 };
	struct KeyEvent {
		UnsignedByte key;
		UnsignedShort state;
		UnsignedInt sequence;
	};
	KeyEvent m_eventQueue[EVENT_BUFFER_SIZE];
	UnsignedInt m_head;
	UnsignedInt m_tail;
	UnsignedInt m_sequence;
	Bool m_capsLock;
};

extern LinuxKeyboard *TheLinuxKeyboard;

#endif // PAL_INPUT_H
