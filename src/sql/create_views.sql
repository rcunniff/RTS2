DROP VIEW targets_images;
DROP VIEW targets_noimages;
DROP VIEW targets_imgcount;
DROP VIEW observations_imgcount;
DROP VIEW observations_noimages;
DROP VIEW observations_images;
DROP VIEW observations_nights;
DROP VIEW images_nights;

CREATE VIEW targets_noimages AS 
SELECT targets.tar_id, 0 AS img_count 
	FROM targets 
	WHERE (NOT (EXISTS 
		(SELECT * FROM images, observations 
			WHERE ((observations.tar_id = targets.tar_id) AND 
			(observations.obs_id = images.obs_id)))));

CREATE VIEW targets_imgcount AS
SELECT targets.tar_id, count(*) AS img_count
	FROM targets, observations, images 
	WHERE images.obs_id = observations.obs_id AND 
		observations.tar_id = targets.tar_id
	GROUP BY targets.tar_id ORDER BY count(*) DESC;

CREATE VIEW targets_images AS 
SELECT tar_id, img_count 
	FROM targets_imgcount 
UNION SELECT tar_id, img_count 
	FROM targets_noimages;

CREATE VIEW targets_altaz AS
SELECT targets.*, 
	obj_alt (tar_ra, tar_dec, 240000, mount_long, mount_lat)
	AS alt, 
	obj_az (tar_ra, tar_dec, 240000, mount_long, mount_lat) 
	AS az
	FROM mounts, targets
	WHERE mount_name = 'T1';

CREATE VIEW observations_noimages AS 
SELECT observations.obs_id, 0 AS img_count 
	FROM observations 
	WHERE (NOT (EXISTS 
		(SELECT * FROM images 
			WHERE observations.obs_id = images.obs_id)));

CREATE VIEW observations_imgcount AS
SELECT observations.obs_id, count(*) AS img_count
	FROM observations, images 
	WHERE images.obs_id = observations.obs_id
	GROUP BY observations.obs_id ORDER BY count(*) DESC;

CREATE VIEW observations_images AS 
SELECT obs_id, img_count 
	FROM observations_imgcount 
UNION SELECT obs_id, img_count 
	FROM observations_noimages;

CREATE VIEW observations_nights AS
SELECT observations.*, date_part('day', (obs_start - interval '12:00')) AS obs_night, 
	date_part('month', (obs_start - interval '12:00')) AS obs_month, 
	date_part('year', (observations.obs_start - interval '12:00')) AS obs_year 
	FROM observations;

CREATE VIEW images_nights AS
SELECT images.*, date_part('day', (timestamptz(images.img_date) - interval '12:00')) AS img_night, 
	date_part('month', (timestamptz(images.img_date) - interval '12:00')) AS img_month, 
	date_part('year', (timestamptz(images.img_date) - interval '12:00')) AS img_year 
	FROM images;

GRANT SELECT ON targets_noimages TO "";
GRANT SELECT ON targets_imgcount TO "";
GRANT SELECT ON targets_images TO "";
GRANT SELECT ON targets_altaz TO "";

GRANT SELECT ON observations_noimages TO "";
GRANT SELECT ON observations_imgcount TO "";
GRANT SELECT ON observations_images TO "";
GRANT SELECT ON observations_nights TO "";
GRANT SELECT ON images_nights TO "";

------------------------------------------------------

GRANT SELECT ON targets_noimages TO GROUP observers;
GRANT SELECT ON targets_imgcount TO GROUP observers;
GRANT SELECT ON targets_images TO GROUP observers;
GRANT SELECT ON targets_altaz TO GROUP observers;

GRANT SELECT ON observations_noimages TO GROUP observers;
GRANT SELECT ON observations_imgcount TO GROUP observers;
GRANT SELECT ON observations_images TO GROUP observers;
GRANT SELECT ON observations_nights TO GROUP observers;
GRANT SELECT ON images_nights TO GROUP observers;

