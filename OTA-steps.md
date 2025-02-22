1. Receiving Phase:
   - Device receives firmware update packets/chunks
   - These are typically stored in a temporary buffer or separate flash partition
   - The device may continue normal operation during download

2. Verification Phase:
   - Cryptographic signature verification to ensure authenticity
   - Checksum validation to ensure integrity
   - Version checking to prevent downgrade attacks
   - Validation of hardware compatibility

3. Preparation Phase:
   - Backup of critical data if needed
   - Ensuring enough space in target memory partition
   - Creating a fallback/recovery point in case update fails

4. Installation Phase:
   - Writing new firmware to secondary/inactive partition
   - Updating boot loader parameters to point to new firmware
   - Setting flags to indicate update is pending

5. Reset/Boot Phase:
   - Device performs controlled restart
   - Bootloader sees pending update flag
   - Boots from new firmware partition
   - Validates successful boot (often with a watchdog timer)
   - If successful, marks update as complete
   - If failed, reverts to previous firmware

---

MVP: do 1, 4 and 5.
