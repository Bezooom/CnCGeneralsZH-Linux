#include "PAL_Input.h"
#include <cstring>
#include <cassert>

LinuxKeyboard *TheLinuxKeyboard = nullptr;

LinuxKeyboard::LinuxKeyboard()
	: Keyboard(),
	  m_head(0),
	  m_tail(0),
	  m_sequence(0),
	  m_capsLock(false)
{
	TheLinuxKeyboard = this;
	std::memset(m_eventQueue, 0, sizeof(m_eventQueue));
}

LinuxKeyboard::~LinuxKeyboard()
{
	if (TheLinuxKeyboard == this) {
		TheLinuxKeyboard = nullptr;
	}
}

Bool LinuxKeyboard::getCapsState()
{
	return m_capsLock ? TRUE : FALSE;
}

void LinuxKeyboard::addKeyEvent(UnsignedByte keyCode, Bool isDown)
{
	UnsignedInt nextTail = (m_tail + 1) % EVENT_BUFFER_SIZE;
	if (nextTail == m_head) {
		m_head = (m_head + 1) % EVENT_BUFFER_SIZE;
	}
	m_eventQueue[m_tail].key = keyCode;
	m_eventQueue[m_tail].state = isDown ? KEY_STATE_DOWN : KEY_STATE_UP;
	m_eventQueue[m_tail].sequence = ++m_sequence;
	m_tail = nextTail;

	if (keyCode == KEY_CAPS && isDown) {
		m_capsLock = !m_capsLock;
	}
}

void LinuxKeyboard::getKey(KeyboardIO *key)
{
	assert(key);
	key->sequence = 0;
	key->key = KEY_NONE;
	key->status = KeyboardIO::STATUS_UNUSED;
	key->state = 0;

	if (m_head != m_tail) {
		key->key = m_eventQueue[m_head].key;
		key->state = m_eventQueue[m_head].state;
		key->sequence = m_eventQueue[m_head].sequence;
		key->status = KeyboardIO::STATUS_UNUSED;
		m_head = (m_head + 1) % EVENT_BUFFER_SIZE;
	}
}

void Platform_AddKeyEvent(unsigned char scanCode, bool isDown)
{
	if (TheLinuxKeyboard) {
		TheLinuxKeyboard->addKeyEvent(scanCode, isDown ? TRUE : FALSE);
	}
}

void Platform_ResetKeys(void)
{
	if (TheKeyboard) {
		TheKeyboard->resetKeys();
	}
}
