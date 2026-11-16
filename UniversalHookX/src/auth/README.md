# Login display name

The login request remains unchanged. Account lookup may continue to ignore case;
the client does not capitalize or otherwise rewrite the entered account name.

After authentication, the server should include the exact account name read from
the matched database row as the fourth field:

```text
OK|daysLeft|expiryDate|username
OK|30|2026-11-04|Guenni
OK|-1|PERMANENT|mIxEd_Name
```

Select the `username` column along with the existing account/license fields and
append that database value, rather than the submitted login parameter. Every
successful branch, including permanent licenses and initial HWID binding, must
use this format. Keep the existing password, HWID and license checks in place.
Do not normalize the database value for display. Failed authentication responses
remain unchanged.

Beta access is an optional fifth field, `1` (beta) or `0` (no beta):

```text
OK|daysLeft|expiryDate|username|beta
OK|30|2026-11-04|Guenni|1
```

Only beta accounts get the BETA badge in the menu footer. A missing field means
no beta; any other value is malformed and drops both the name and the flag.

Legacy `OK`, `OK|daysLeft`, and `OK|daysLeft|expiryDate` responses remain supported.
They have no canonical name, so the menu falls back to the entered spelling.
The expiry field is parsed separately from the optional name. Empty, oversized,
control-character or extra-delimiter name metadata is ignored.

The live auth-server source is outside this repository. The old server.js found
in the injector build folder has no license response and must not be assumed to
represent the deployed server.
