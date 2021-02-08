CREATE TABLE consumer_properties (
    interface varchar not null,
    path varchar,
    wave blob,
    PRIMARY KEY (interface, path)
)
