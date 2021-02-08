CREATE TABLE producer_properties (
    interface varchar not null,
    path varchar,
    payload blob,
    PRIMARY KEY (interface, path)
)
