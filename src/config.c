/*****************************************
        NanoShell Operating System
          (C) 2022 iProgramInCpp

        Kernel Configuration module
******************************************/
#include <config.h>

uint32_t val_32_const = 0x811c9dc5u;
uint32_t prime_32_const = 0x1000193u;

//TODO: I stole this hashing algorithm from the internet.
//Should make it non-recursive

static uint32_t FNV321(const char *const str, uint32_t value)
{
	return (*str == '\0') ? value : FNV321(str + 1, (value ^ (uint32_t)(*str)) * prime_32_const);
}
uint32_t HashString(const char *const str)
{
    return FNV321 (str, val_32_const);
}
static bool IsSpace (char c)
{
	return (c == '\t' || c == ' ');
}

ConfigEntry* g_config_entries = NULL;
int          g_config_entries_count = 0;
int          g_config_entries_max   = 512;

void CfgInitialize()
{
	size_t sz = sizeof (ConfigEntry) * g_config_entries_max;
	g_config_entries = (ConfigEntry*)MmAllocateK (sz);
	memset (g_config_entries, 0, sz);
}

//Append
ConfigEntry* CfgGetEntry(const char* key)
{
	uint32_t entry_hash = HashString (key);
    int l = 0, r = g_config_entries_count;
    while (l < r)
    {
        int m = (l+r) / 2;
		if (g_config_entries[m].entry_hash == entry_hash)
			return &g_config_entries[m];
        if (g_config_entries[m].entry_hash >  entry_hash)
        {
            r = m;
        }
        else
        {
            l = m + 1;
        }
    }
	return NULL;
}
ConfigEntry* CfgAddEntry(ConfigEntry *pEntry)
{
	if (g_config_entries_count >= g_config_entries_max)
	{
		LogMsg("Couldn't add config `%s=%s`, too many config parms already specified", pEntry->entry, pEntry->value);
		return NULL;
	}
	ConfigEntry *p = CfgGetEntry (pEntry->entry);
	if (p)
	{
		*p = *pEntry;
		return p;
	}
    int l = 0, r = g_config_entries_count, spot = 0;
    while (l < r)
    {
        int m = (l+r) / 2;
        if (g_config_entries[m].entry_hash > pEntry->entry_hash)
        {
            r = m;
        }
        else
        {
            l = m + 1;
        }
    }
    spot = l;
    for (int i = g_config_entries_count; i > spot; i--)
    {
        g_config_entries[i] = g_config_entries[i-1];
    }
    g_config_entries[spot] = *pEntry;
    g_config_entries_count ++;
    return &g_config_entries[spot];
}
void CfgPrintEntries()
{
    LogMsg("Displaying entries:");
    for (int i = 0; i < g_config_entries_count; i++)
    {
        LogMsg("%s: '%s'", g_config_entries[i].entry, g_config_entries[i].value);
    }
}

//strings are of format:
void CfgLoadFromTextBasic(char* work)
{
    TokenState state;
    memset (&state, 0, sizeof state);

    char namesp[32]; uint32_t namesp_hash = 0;
    namesp[0] = 0;

    char *p = Tokenize (&state, work, "\n");
    do
    {
        while (IsSpace(*p)) p++;
        if (*p == '/' || *p == '#' || *p == '\n')
        {
            p = Tokenize (&state, NULL, "\n");
            continue;
        }

        TokenState istate;
        memset (&istate, 0, sizeof istate);

        // Get the first part, and trim its spaces off
        char *key = Tokenize (&istate, p, "=");
        while (IsSpace(*key)) key++;
        char *lkey = key + strlen (key) - 1;
        while (IsSpace(*lkey) && lkey > key) { *lkey = 0; lkey--; }

        if (*key == '[')
        {
            // Namespace name
            lkey = key + strlen (key) - 1;
            if (*lkey == ']') {*lkey = 0; lkey--; }

            strcpy (namesp, key + 1);

            // No, continue
            p = Tokenize (&state, NULL, "\n");
            continue;
        }

        // Get the second part, and trim its spaces off
        char *val = istate.m_pContinuation;

        // Before we do that though, check if we have one
        if (!val)
        {
            // No, continue
            p = Tokenize (&state, NULL, "\n");
            continue;
        }

        while (IsSpace(*val)) val++;

        char *lval = val + strlen (val) - 1;
        while (IsSpace(*lval) && lval > val) { *lval = 0; lval--; }

        ConfigEntry entry;
        memset(&entry, 0, sizeof entry);
        strcpy (entry.value, val);
		strcpy (entry.entry, namesp);
		if (*namesp)
		{
			strcat (entry.entry, "::");
		}
		strcat (entry.entry, key);
		
        entry.entry_hash  = HashString(entry.entry);
        CfgAddEntry(&entry);

        p = Tokenize (&state, NULL, "\n");
    }
    while (p);
}

void CfgLoadFromText(const char* parms)
{
	int sl = strlen (parms);
    char *work = MmAllocateK (sl + 1);//strdup (parms);
    if (!work)
    {
        LogMsg("CfgLoadFromText failure");
        return;
    }
	strcpy (work, parms);
    CfgLoadFromTextBasic(work);

    MmFreeK (work);
}

void CfgLoadFromParms(const char* parms)
{
	int sl = strlen (parms);
    char *work = MmAllocateK (sl + 1);//strdup (parms);
    if (!work)
    {
        LogMsg("CfgLoadFromParms failure");
        return;
    }
	strcpy (work, parms);

    TokenState state;
    memset (&state, 0, sizeof state);

    char *p = Tokenize (&state, work, " ");
    do
    {
        TokenState istate;
        memset (&istate, 0, sizeof istate);

        char *key = Tokenize (&istate, p, "=");
        char *val = istate.m_pContinuation;

        if (!key || !val)
        {
            LogMsg("CfgLoadFromParms invalid parm string %s", p);
            return;
        }

        ConfigEntry entry;
        memset(&entry, 0, sizeof entry);
        strcpy (entry.value, val);
		strcpy (entry.entry, key);
        entry.entry_hash = HashString(entry.entry);
        CfgAddEntry(&entry);

        p = Tokenize (&state, NULL, " ");
    }
    while (p);

    MmFreeK (work);
}