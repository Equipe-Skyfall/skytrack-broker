Mosquitto config for SkyTrack

This folder contains Mosquitto configuration and helper files used by the docker-compose setup.

Files and purpose:
- mosquitto.conf - main configuration (this file)
- passwords (not committed) - mosquitto password file generated with mosquitto_passwd
- acl (optional) - ACL file to restrict topics per user

How to create a password file (recommended):

On your host machine (needs mosquitto-clients installed) run:

```bash
# create or update 'passwords' and add user 'skyuser'
mosquitto_passwd -c ./mosquitto/config/passwords skyuser
# the command will prompt for a password and create the file
```

If you want to add additional users (without recreating file):

```bash
mosquitto_passwd ./mosquitto/config/passwords anotheruser
```

Notes:
- The `docker-compose.yml` mounts `./mosquitto/config` into the container at `/mosquitto/config` in read-only mode.
  Make sure you create the `passwords` file locally before starting the compose stack.
- If you prefer not to require authentication for development, you can set `allow_anonymous true` in `mosquitto.conf`.
- To enable topic-level access control, create an `acl` file and add `acl_file /mosquitto/config/acl` to `mosquitto.conf`.
