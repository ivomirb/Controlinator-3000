#pragma once

#if USE_SHARED_STATE
DialogScreen::ActiveState *DialogScreen::GetActiveState( void )
{
	Assert(IsActive());
	return &g_ScreenTimeshare.dialog;
}
#endif

void DialogScreen::DrawLine( uint8_t row, bool bCenter )
{
	const char *line = GetActiveState()->m_Lines[row];
	DrawTextXY(bCenter ? (128 - Strlen(line) * 7) / 2 : 1, g_Rows[row], line);
}

void DialogScreen::Draw( void )
{
	auto *pState = GetActiveState();

#if PARTIAL_SCREEN_UPDATE
	DrawState *pDrawState = reinterpret_cast<DrawState*>(s_DrawState.custom);
	const bool bDrawAll = s_DrawState.bDrawAll || pDrawState->id != pState->m_Id || pDrawState->checkFlags != pState->m_CheckFlags;
	pDrawState->id = pState->m_Id;
	pDrawState->checkFlags = pState->m_CheckFlags;
	if (!bDrawAll) return;
	if (!s_DrawState.bDrawAll)
	{
		ClearBuffer();
	}
#endif

	if (pState->m_Lines[0][0])
	{
		DrawBox(0, 10, 128, 1);
		DrawLine(0, true);
	}
	else
	{
		DrawMachineStatus(nullptr, 0);
	}

	DrawLine(1, (pState->m_AlignFlags&1) == 0);
	DrawLine(2, (pState->m_AlignFlags&2) == 0);
	DrawLine(3, (pState->m_AlignFlags&4) == 0);

	if (pState->m_CheckFlags == 0 && pState->m_Buttons[0][0] != '@')
	{
		DrawButton(BUTTON_1, pState->m_Buttons[0], Strlen(pState->m_Buttons[0]), TestBit(pState->m_ButtonHoldFlags, 0));
	}

	DrawButton(BUTTON_2, pState->m_Buttons[1], Strlen(pState->m_Buttons[1]), TestBit(pState->m_ButtonHoldFlags, 1));
}

void DialogScreen::Update( unsigned long time )
{
	auto *pState = GetActiveState();

	if (pState->m_ButtonDismissTimer != 0 && time - pState->m_ButtonDismissTimer > 500)
	{
		CloseScreen();
		return;
	}

	int8_t button = GetCurrentButton();
	if (button >= 0 && button < 3 && pState->m_Lines[button + 1][0] == CHAR_UNCHECKED)
	{
		pState->m_Lines[button + 1][0] = CHAR_CHECKED; // switch checkbox to checked
		pState->m_CheckFlags &= ~(1 << button);
	}
	else if (button >= 0 && button <= 3 && pState->m_CheckFlags)
	{
		return;
	}
	if (pState->m_Buttons[0][0] == '@' && pState->m_CheckFlags == 0 && g_ButtonState == 0)
	{
		SendResponse(1);
		if (TestBit(pState->m_ButtonWaitFlags, 0))
		{
			pState->m_ButtonDismissTimer = time;
		}
		else
		{
			CloseScreen();
		}
		return;
	}

	for (uint8_t i = 0; i < 2; i++)
	{
		uint8_t b = i ? BUTTON_2 : BUTTON_1;
		if (TestBit(pState->m_ButtonHoldFlags, i))
		{
			if (!TestBit(g_ButtonHold, b))
			{
				continue;
			}
		}
		else if (button != b)
		{
			continue;
		}
		SendResponse(i + 1);
		if (TestBit(pState->m_ButtonWaitFlags, i))
		{
			pState->m_ButtonDismissTimer = time;
		}
		else
		{
			CloseScreen();
		}
		return;
	}
}

void DialogScreen::Deactivate( void )
{
	auto *pState = GetActiveState();
	if (pState->m_Id)
	{
		SendResponse(0);
	}
}

void DialogScreen::SendResponse( uint8_t button )
{
	auto *pState = GetActiveState();
	Serial.print(ROMSTR("DIALOG:"));
	Serial.print(pState->m_Id);
	Serial.print(g_StrComma);
	Serial.println(button);
	pState->m_Id = 0;
}

// Parses the dialog description: <dialog id>|<title>|<line1>|<line2>|<line3>|<left button>,<right button>
void DialogScreen::ParseDialog( const char *str )
{
	auto *pState = GetActiveState();
	pState->m_CheckFlags = 0;
	pState->m_AlignFlags = 0;
	pState->m_ButtonHoldFlags = 0;
	pState->m_ButtonWaitFlags = 0;
	pState->m_ButtonDismissTimer = 0;

	// parse id
	pState->m_Id = atol(str);
	str = strchr(str, '|') + 1;

	// parse lines
	for (uint8_t i = 0; i < 4; i++)
	{
		if (i > 0 && *str == '^')
		{
			pState->m_AlignFlags |= 1 << (i - 1);
			str++;
		}
		if (i > 0 && *str == CHAR_UNCHECKED)
		{
			pState->m_CheckFlags |= 1 << (i - 1);
			pState->m_AlignFlags |= 1 << (i - 1);
		}
		const char *end = strchr(str, '|');
		uint8_t len = (uint8_t)(end - str);
		if (len > 18) len = 18;
		memcpy(pState->m_Lines[i], str, len);
		pState->m_Lines[i][len] = 0;
		str = end + 1;
	}

	// parse buttons
	const char *end = strchr(str, ',');
	uint8_t len = (uint8_t)(end - str);
	if (*str == CHAR_HOLD)
	{
		pState->m_ButtonHoldFlags |= 1;
		str++;
		len--;
	}
	if (*str == '!')
	{
		pState->m_ButtonWaitFlags |= 1;
		str++;
		len--;
	}
	if (len > MAX_BUTTON_SIZE) len = MAX_BUTTON_SIZE;
	memcpy(pState->m_Buttons[0], str, len);
	pState->m_Buttons[0][len] = 0;
	str = end + 1;

	len = Strlen(str);
	if (*str == '!')
	{
		pState->m_ButtonWaitFlags |= 2;
		str++;
		len--;
	}
	if (len > 0 && str[len - 1] == CHAR_HOLD)
	{
		pState->m_ButtonHoldFlags |= 2;
		len--;
	}
	if (len > MAX_BUTTON_SIZE) len = MAX_BUTTON_SIZE;
	memcpy(pState->m_Buttons[1], str, len);
	pState->m_Buttons[1][len] = 0;
}
